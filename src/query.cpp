#include <asiopq/exception.hpp>
#include <asiopq/query.hpp>
#include <libpq-fe.h>
#include <stdexcept>


namespace asiopq {


	void query::flush (native_handle_type handle) {

		switch (PQflush(handle)) {

			default:
				throw connection_error(handle);
			case 0:
				flushed_=true;
				break;
			case 1:
				break;

		}

	}


	query::operation_status query::get_status () const noexcept {

		//	If [PQflush] returns 1, wait for the socket to become read- or write-ready.
		//
		//	Once PQflush returns 0, wait for the socket to be read-ready and then
		//	read the response as described above.
		return flushed_ ? operation_status::read : operation_status::read_write;

	}


	query::query (timeout_type timeout) noexcept : timeout_(timeout), flushed_(false) {	}


	void query::result (native_result_type result) {

		PQclear(result);

		throw std::logic_error("Received result from PostgreSQL where one was not expected");

	}


	query::operation_status query::begin (native_handle_type handle) {

		send(handle);

		//	After sending any command or data on a nonblocking connection, call PQflush.
		flush(handle);

		return get_status();

	}


	static void consume (query::native_handle_type handle) {

		if (PQconsumeInput(handle)==0) throw connection_error(handle);

	}


	query::operation_status query::perform (native_handle_type handle, socket_status status) {

		if (!flushed_) {

			//	If it becomes read-ready, call PQconsumeInput...
			if (status==socket_status::readable) consume(handle);
			//	...then call PQflush again.
			//
			//	If it becomes write-ready, call PQflush again.
			flush(handle);

			return get_status();

		}

		//	When the main loop detects input ready, it should call PQconsumeInput to read
		//	the input.
		consume(handle);

		//	It can then call PQisBusy, followed by PQgetResult if PQisBusy returns false (0).
		while (PQisBusy(handle)==0) {

			auto res=PQgetResult(handle);
			//	PQgetResult must be called repeatedly until it returns a null pointer,
			//	indicating that the command is done.
			if (!res) return operation_status::done;

			result(res);

		}

		return operation_status::read;

	}


	query::timeout_type query::timeout () {

		return timeout_;

	}


}
