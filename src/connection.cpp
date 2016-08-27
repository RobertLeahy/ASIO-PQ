#include <asiopq/asio.hpp>
#include <asiopq/connection.hpp>
#include <asiopq/exception.hpp>
#include <asiopq/operation.hpp>
#include <asiopq/scope.hpp>
#include <libpq-fe.h>
#include <chrono>
#include <exception>
#include <memory>
#include <stdexcept>
#include <system_error>
#include <utility>


#ifdef _WIN32
#include <Winsock2.h>
#include <Windows.h>
#else
#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>
#endif


namespace asiopq {


	connection::control::control (connection & self, asio::io_service & ios) : stop_(false), self_(&self), timer(ios) {	}


	connection::control::guard_type connection::control::lock () const noexcept {

		return guard_type(m_);

	}


	void connection::control::stop () noexcept {

		stop_=true;
		timer.cancel();

	}


	connection::control::operator bool () const noexcept {

		return !stop_;

	}


	void connection::control::move (connection & self) noexcept {

		self_=&self;

	}


	connection & connection::control::self () const noexcept {

		return *self_;

	}


	void connection::update_socket () {

		auto s=PQsocket(handle_);
		if (s==-1) {

			socket_.close();
			return;

		}

		#ifdef _WIN32

		WSAPROTOCOL_INFOW info;
		if (WSADuplicateSocketW(
			s,
			GetProcessId(GetCurrentProcess()),
			&info
		)==SOCKET_ERROR) throw std::system_error(
			std::error_code(
				GetLastError(),
				std::system_category()
			)
		);

		if (socket_.is_open() && (sid_==info.dwCatalogEntryId)) return;

		auto n=WSASocketW(
			info.iAddressFamily,
			info.iSocketType,
			info.iProtocol,
			&info,
			0,
			WSA_FLAG_OVERLAPPED
		);
		if (n==INVALID_SOCKET) throw std::system_error(
			std::error_code(
				GetLastError(),
				std::system_category()
			)
		);
		s=n;
		auto g=make_scope_exit([&] () noexcept {	closesocket(s);	});
		bool is_v6=info.iAddressFamily!=AF_INET;

		#else

		struct sockaddr info;
		socklen_t len=sizeof(info);
		if (
			(getsockname(s,&info,&len)!=0) ||
			((s=dup(s))==-1)
		) throw std::system_error(
			std::error_code(
				errno,
				std::system_category()
			)
		);
		auto g=make_scope_exit([&] () noexcept {	close(s);	});
		bool is_v6=info.sa_family!=AF_INET;

		#endif

		socket_.assign(is_v6 ? asio::ip::tcp::v6() : asio::ip::tcp::v4(),s);
		g.release();

		#ifdef _WIN32

		sid_=info.dwCatalogEntryId;

		#endif

	}


	template <typename F>
	auto connection::wrap (F && functor) noexcept(std::is_nothrow_move_constructible<F>::value) {

		return [&,control=control_,op=op_,functor=std::forward<F>(functor)] (auto &&... args) mutable {

			auto l=control->lock();
			if (!*control) return;
			auto & self=control->self();
			if (op!=self.op_) return;

			return std::forward<F>(functor)(self,std::forward<decltype(args)>(args)...);

		};

	}


	void connection::next () {

		socket_.cancel();
		control_->timer.cancel();
		read_=false;
		write_=false;
		op_=operation_type{};

		if (pending_.empty()) return;

		op_=std::move(pending_.front());
		pending_.pop_front();

		begin();

	}


	void connection::begin () {

		//	Loop until we need to dispatch an asynchronous
		//	operation
		do {

			operation::operation_status status;
			std::exception_ptr ex;
			try {

				status=op_->begin(handle_);

			} catch (...) {

				ex=std::current_exception();

			}

			update_socket();

			if (ex || (status==operation::operation_status::done)) {

				op_->complete(std::move(ex));
				continue;

			}

			//	Setup timeout if applicable
			auto ms=op_->timeout();
			if (ms) {

				auto duration=std::chrono::duration_cast<asio::steady_timer::duration>(*ms);
				control_->timer.expires_from_now(duration);
				control_->timer.async_wait(wrap([ms=*ms] (auto & self, const auto &) {
					
					self.op_->complete(std::make_exception_ptr(timed_out(ms)));
					self.next();
				
				}));

			}

			//	Dispatch read and/or write
			dispatch(status);

			//	Done
			break;

		} while (next(),op_);

	}


	void connection::dispatch (operation::operation_status status) {

		//	Dispatch read if applicable
		switch (status) {

			default:
				break;
			case operation::operation_status::read:
			case operation::operation_status::read_write:
				//	There's a read pending, don't need to dispatch
				//	another
				if (read_) break;
				socket_.async_read_some(asio::null_buffers{},wrap([] (auto & self, const auto &, auto) {

					self.read_=false;
					self.perform(operation::socket_status::readable);

				}));
				read_=true;

		}

		//	Dispatch write if applicable
		switch (status) {

			default:
				break;
			case operation::operation_status::write:
			case operation::operation_status::read_write:
				//	There's a write pending, don't need to dispatch
				//	another
				if (write_) break;
				socket_.async_write_some(asio::null_buffers{},wrap([] (auto & self, const auto &, auto) {

					self.write_=false;
					self.perform(operation::socket_status::writable);

				}));
				write_=true;
				break;

		}

	}


	void connection::perform (operation::socket_status status) {

		operation::operation_status result;
		std::exception_ptr ex;
		try {

			result=op_->perform(handle_,status);

		} catch (...) {

			ex=std::current_exception();

		}

		update_socket();

		if (ex || (result==operation::operation_status::done)) {

			op_->complete(std::move(ex));
			next();
			return;

		}

		dispatch(result);

	}


	connection::connection (native_handle_type handle, asio::io_service & ios)
		:	handle_(handle),
			ios_(ios),
			socket_(ios),
			control_(std::make_shared<control>(*this,ios)),
			read_(false),
			write_(false)
	{

		update_socket();

		if (PQsetnonblocking(handle_,1)!=0) throw connection_error(handle_);

	}


	connection::connection (connection && rhs) noexcept
		:	handle_(rhs.handle_),
			ios_(rhs.ios_),
			socket_(ios_),
			read_(rhs.read_),
			write_(rhs.write_)
	{

		auto l=rhs.control_->lock();

		try {

			op_=std::move(rhs.op_);
			pending_=std::move(rhs.pending_);
			socket_=std::move(rhs.socket_);
			using std::swap;
			swap(control_,rhs.control_);

		} catch (...) {

			//	We cannot allow the lock to be released
			//	or there could be UB
			std::terminate();

		}

		control_->move(*this);

	}


	connection::~connection () noexcept {

		if (!control_) return;

		auto l=control_->lock();

		//	This prevents asynchronous callbacks from proceeding
		//	which prevents undefined behaviour
		control_->stop();

		//	Inform all operations that they will not complete
		auto ex=std::make_exception_ptr(aborted{});
		for (auto && ptr : pending_) ptr->complete(ex);
		if (op_) op_->complete(std::move(ex));

		PQfinish(handle_);

	}


	void connection::add (operation_type op) {

		auto l=control_->lock();

		//	If an operation is running this new operation
		//	becomes pending
		if (op_) {

			pending_.push_back(std::move(op));
			return;

		}

		//	No pending operation: This operation is now
		//	the current operation
		using std::swap;
		swap(op_,op);
		auto g=make_scope_exit([&] () noexcept {	swap(op_,op);	});

		ios_.post(wrap([] (auto & self) {	self.begin();	}));

		g.release();

	}


	asio::io_service & connection::get_io_service () const noexcept {

		return ios_;

	}


	connection::native_handle_type connection::native_handle () const noexcept {

		return handle_;

	}


}
