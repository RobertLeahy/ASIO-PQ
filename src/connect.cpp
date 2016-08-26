#include <asiopq/configure.hpp>
#include <asiopq/connect.hpp>
#include <asiopq/connection.hpp>
#include <asiopq/exception.hpp>
#include <asiopq/scope.hpp>
#include <libpq-fe.h>
#include <exception>
#include <new>
#include <stdexcept>
#include <utility>


#ifdef ASIOPQ_USE_BOOST_FUTURE
#include <boost/exception_ptr.hpp>
#endif


namespace asiopq {


	void connect::init () {

		if (!handle_) throw std::bad_alloc{};
		auto g=make_scope_exit([&] () noexcept {	PQfinish(handle_);	});

		if (PQstatus(handle_)==CONNECTION_BAD) throw connection_error(handle_);

		g.release();

	}


	connect::connect (const char * const * keywords, const char * const * values, int expand_dbname)
		:	handle_(PQconnectStartParams(keywords,values,expand_dbname))
	{

		init();

	}


	connect::connect (const char * conninfo) : handle_(PQconnectStart(conninfo)) {

		init();

	}


	connect::~connect () noexcept {

		if (handle_) PQfinish(handle_);

	}


	connection connect::connection (asio::io_service & ios) {

		if (!handle_) throw std::logic_error("Object does not manage a Postgres connection");

		asiopq::connection retr(handle_,ios);
		handle_=nullptr;

		retr.add(shared_from_this());

		return retr;

	}


	void connect::complete (std::exception_ptr ex) {

		if (ex_) {

			std::exception_ptr ptr;
			using std::swap;
			swap(ptr,ex_);
			ex=std::move(ptr);

		} else if (!ex) {

			promise_.set_value();
			return;

		}

		#ifdef ASIOPQ_USE_BOOST_FUTURE

		try {

			std::rethrow_exception(std::move(ex));

		} catch (...) {

			promise_.set_exception(boost::current_exception());

		}

		#else

		promise_.set_exception(std::move(ex));

		#endif

	}


	connect::operation_status connect::begin (native_handle_type) {

		//	If you have yet to call PQconnectPoll,
		//	i.e., just after the call to PQconnectStart,
		//	behave as if it last returned PGRES_POLLING_WRITING.
		return operation_status::write;

	}


	connect::operation_status connect::perform (native_handle_type handle, socket_status) {

		switch (PQconnectPoll(handle)) {

			case PGRES_POLLING_WRITING:
				return operation_status::write;
			case PGRES_POLLING_READING:
				return operation_status::read;
			case PGRES_POLLING_OK:
				return operation_status::done;
			default:
				ex_=std::make_exception_ptr(connection_error(handle));
				return operation_status::done;

		}

	}


	connect::timeout_type connect::timeout () {

		return timeout_type(in_place,1000);

	}


	future<void> connect::get_future () {

		return promise_.get_future();

	}


}
