#include <asiopq/exception.hpp>
#include <libpq-fe.h>
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


}
