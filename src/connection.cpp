#include <asiopq/asio.hpp>
#include <asiopq/connection.hpp>
#include <asiopq/exception.hpp>
#include <asiopq/operation.hpp>
#include <asiopq/scope.hpp>
#include <libpq-fe.h>
#include <chrono>
#include <exception>
#include <memory>
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


	connection::control::control () : stop_(false) {	}


	connection::control::guard_type connection::control::lock () const noexcept {

		return guard_type(m_);

	}


	void connection::control::stop () noexcept {

		stop_=true;

	}


	connection::control::operator bool () const noexcept {

		return !stop_;

	}


	void connection::update_socket () {

		auto s=PQsocket(handle_);
		if (s<0) {

			socket_.close();
			return;

		}

		if (socket_.is_open()) {

			#pragma GCC diagnostic push
			#pragma GCC diagnostic ignored "-Wsign-compare"
			if (socket_.native_handle()==s) return;
			#pragma GCC diagnostic pop

		}

		#ifdef _WIN32

		WSAPROTOCOL_INFOW info;
		s=WSADuplicateSocketW(
			s,
			GetProcessId(GetCurrentProcess()),
			&info
		);
		if (s==SOCKET_ERROR) throw std::system_error(
			std::error_code(
				GetLastError(),
				std::system_category()
			)
		);
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

	}


	template <typename F>
	auto connection::wrap (F && functor) noexcept(std::is_nothrow_move_constructible<F>::value) {

		return [&,control=control_,op=op_,functor=std::forward<F>(functor)] (auto &&... args) mutable {

			auto l=control->lock();
			if (!*control) return;
			if (op!=this->op_) return;

			return std::forward<F>(functor)(std::forward<decltype(args)>(args)...);

		};

	}


	void connection::next () {

		socket_.cancel();
		timer_.cancel();
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
			try {

				status=op_->begin(handle_);

			} catch (...) {

				op_->complete(std::current_exception());
				continue;

			}

			if (status==operation::operation_status::done) {

				op_->complete({});
				continue;

			}

			//	Setup timeout if applicable
			auto ms=op_->timeout();
			if (ms) {

				auto duration=std::chrono::duration_cast<asio::steady_timer::duration>(*ms);
				timer_.expires_from_now(duration);
				timer_.async_wait(wrap([&,ms=*ms] (const auto &) {
					
					this->op_->complete(std::make_exception_ptr(timed_out(ms)));
					this->next();
				
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
				socket_.async_read_some(asio::null_buffers{},[&] (const auto &, auto) {

					this->read_=false;
					this->perform(operation::socket_status::readable);

				});
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
				socket_.async_write_some(asio::null_buffers{},[&] (const auto &, auto) {

					this->write_=false;
					this->perform(operation::socket_status::writable);

				});
				write_=true;
				break;

		}

	}


	void connection::perform (operation::socket_status status) {

		operation::operation_status result;
		try {

			result=op_->perform(handle_,status);

		} catch (...) {

			op_->complete(std::current_exception());
			next();
			return;

		}

		if (result==operation::operation_status::done) {

			op_->complete({});
			next();
			return;

		}

		dispatch(result);

	}


	connection::connection (native_handle_type handle, asio::io_service & ios)
		:	handle_(handle),
			ios_(ios),
			socket_(ios),
			control_(std::make_shared<control>()),
			read_(false),
			write_(false),
			timer_(ios)
	{

		update_socket();

		if (PQsetnonblocking(handle_,1)!=0) throw connection_error(handle_);

	}


	connection::~connection () noexcept {

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

		ios_.post(wrap([&] () {	begin();	}));

		g.release();

	}


}
