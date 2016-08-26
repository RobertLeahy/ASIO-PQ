/**
 *	\file
 */


#pragma once


#include "future.hpp"
#include "operation.hpp"
#include <exception>


namespace asiopq {


	/**
	 *	Represents the operation of resetting the
	 *	connection to a Postgres database.
	 *
	 *	For more information see the libpq documentation
	 *	of the PQresetStart and PQresetPoll functions.
	 */
	class reset : public operation {


		private:


			promise<void> promise_;
			timeout_type timeout_;


		public:


			/**
			 *	Creates a reset object.
			 *
			 *	\param [in] timeout
			 *		A \ref operation::timeout_type object giving
			 *		the amount of time this operation is permitted
			 *		to take at maximum.  Defaults to no timeout
			 *		which means this operation may take infinitely
			 *		long.
			 */
			explicit reset (timeout_type timeout=timeout_type{});


			virtual void complete (std::exception_ptr) override;
			virtual operation_status begin (native_handle_type) override;
			virtual operation_status perform (native_handle_type, socket_status) override;
			virtual timeout_type timeout () override;


			/**
			 *	Retrieves a future which will be completed when
			 *	this operation completes or is aborted.
			 *
			 *	\return
			 *		A future.
			 */
			future<void> get_future ();


	};


}
