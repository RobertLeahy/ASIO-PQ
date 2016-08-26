#include <asiopq/exception.hpp>
#include <asiopq/future.hpp>
#include <asiopq/reset.hpp>
#include <utility>


namespace asiopq {


	reset::reset (timeout_type timeout) : timeout_(timeout) {	}


	void reset::complete (std::exception_ptr ex) {

		if (ex) set_exception(promise_,std::move(ex));
		else promise_.set_value();

	}


	reset::operation_status reset::begin (native_handle_type handle) {

		if (PQresetStart(handle)==0) throw connection_error(handle);

		//	From the documentation for PQresetStart:
		//
		//	If it returns 1, poll the reset using PQresetPoll
		//	in exactly the same way as you would create the
		//	connection using PQconnectPoll.
		//
		//	From the documentation for PQconnectPoll:
		//
		//	If you have yet to call PQconnectPoll, i.e., just
		//	after the call to PQconnectStart, behave as if it
		//	last returned PGRES_POLLING_WRITING.
		return operation_status::write;

	}


	reset::operation_status reset::perform (native_handle_type handle, socket_status) {

		switch (PQresetPoll(handle)) {

			case PGRES_POLLING_WRITING:
				return operation_status::write;
			case PGRES_POLLING_READING:
				return operation_status::read;
			case PGRES_POLLING_OK:
				return operation_status::done;
			default:
				throw connection_error(handle);

		}

	}


	reset::timeout_type reset::timeout () {

		return timeout_;

	}


	future<void> reset::get_future () {

		return promise_.get_future();

	}


}
