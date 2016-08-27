#include <asiopq/configure.hpp>
#include <asiopq/connect.hpp>
#include <asiopq/connection.hpp>
#include <asiopq/exception.hpp>
#include <asiopq/future.hpp>
#include <asiopq/scope.hpp>
#include <libpq-fe.h>
#include <exception>
#include <new>
#include <stdexcept>
#include <utility>


namespace asiopq {


	void connect::init () {

		if (!handle_) throw std::bad_alloc{};
		auto g=make_scope_exit([&] () noexcept {	PQfinish(handle_);	});

		status_=PQstatus(handle_);
		if (status_==CONNECTION_BAD) throw connection_error(handle_);

		g.release();

	}


	connect::connect (const char * const * keywords, const char * const * values, int expand_dbname, timeout_type timeout)
		:	handle_(PQconnectStartParams(keywords,values,expand_dbname)),
			timeout_(timeout)
	{

		init();

	}


	connect::connect (const char * conninfo, timeout_type timeout) : handle_(PQconnectStart(conninfo)), timeout_(timeout) {

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

		if (ex) set_exception(promise_,std::move(ex));
		else promise_.set_value();

	}


	connect::operation_status connect::begin (native_handle_type) {

		//	If you have yet to call PQconnectPoll,
		//	i.e., just after the call to PQconnectStart,
		//	behave as if it last returned PGRES_POLLING_WRITING.
		return operation_status::write;

	}


	connect::operation_status connect::perform (native_handle_type handle, socket_status) {

		connect::operation_status retr;
		switch (PQconnectPoll(handle)) {

			case PGRES_POLLING_WRITING:
				retr=operation_status::write;
				break;
			case PGRES_POLLING_READING:
				retr=operation_status::read;
				break;
			case PGRES_POLLING_OK:
				return operation_status::done;
			default:
				throw connection_error(handle);

		}

		status_=PQstatus(handle);
		status(status_);

		return retr;

	}


	connect::timeout_type connect::timeout () {

		return timeout_;

	}


	future<void> connect::get_future () {

		return promise_.get_future();

	}


	void connect::status (native_status_type) {	}


}
