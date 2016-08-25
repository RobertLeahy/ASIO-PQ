#include <asiopq/exception.hpp>
#include <libpq-fe.h>
#include <sstream>
#include <string>


namespace asiopq {


	static std::string get_error_message (std::string msg) {

		while (!msg.empty()) {

			auto last=msg.back();
			if ((last=='\n') || (last=='\r')) msg.pop_back();
			else break;

		}

		return msg;

	}


	connection_error::connection_error (native_handle_type handle) : error(get_error_message(PQerrorMessage(handle))) {	}


	aborted::aborted () : error("Operation aborted") {	}


	static std::string get_timed_out_message (operation::timeout_type::value_type timeout) {

		std::ostringstream ss;
		using ratio=decltype(timeout)::period;
		static_assert((ratio::num==1) && (ratio::den==1000),"operation::timeout_type does not represent milliseconds");
		ss << "Operation exceeded timeout of " << timeout.count() << "ms";

		return ss.str();

	}


	timed_out::timed_out (operation::timeout_type::value_type timeout) : error(get_timed_out_message(timeout)), timeout_(timeout) {	}


	operation::timeout_type::value_type timed_out::timeout () const noexcept {

		return timeout_;

	}


}
