/**
 *	\file
 */


#pragma once


#include "operation.hpp"
#include <libpq-fe.h>


namespace asiopq {


	/**
	 *	An abstract base class for all queries, commands,
	 *	et cetera sent through libpq.
	 */
	class query : public operation {


		public:


			/**
			 *	The type used by libpq to represent the result
			 *	of a query, command, et cetera.
			 */
			using native_result_type=PGresult *;


		private:


			timeout_type timeout_;
			bool flushed_;


			void flush (native_handle_type);
			operation_status get_status () const noexcept;


		public:


			/**
			 *	Creates a new query object.
			 *
			 *	\param [in] timeout
			 *		A \ref operation::timeout_type object giving the
			 *		amount of time this operation is permitted to
			 *		take at maximum.  Defaults to no timeout which
			 *		means this operation may take infinitely long.
			 */
			explicit query (timeout_type timeout=timeout_type{}) noexcept;


			/**
			 *	Sends the query, command, et cetera to the server.
			 *
			 *	\param [in] handle
			 *		A handle to the current libpq connection.
			 */
			virtual void send (native_handle_type handle) = 0;
			/**
			 *	Invoked for each result returned by the server.
			 *
			 *	Note that if the server does not return a result
			 *	for the query, command, et cetera this method will
			 *	never be invoked.
			 *
			 *	This method is expected to assume ownership of
			 *	\em result even if it throws.  Under no circumstances
			 *	will the invoker call PQclear on \em result.
			 *
			 *	The default implementation of this method throws
			 *	std::logic_error.  The assumption is that if a
			 *	derived class does not override this method they
			 *	do not expect results and therefore obtaining such
			 *	results from the server indicates a serious flaw
			 *	in application logic.
			 *
			 *	\param [in] result
			 *		A libpq handle to the result.
			 */
			virtual void result (native_result_type result);


			virtual operation_status begin (native_handle_type) override;
			virtual operation_status perform (native_handle_type, socket_status) override;
			virtual timeout_type timeout () override;


	};


}
