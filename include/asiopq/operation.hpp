/**
 *	\file
 */


#pragma once


#include "optional.hpp"
#include <libpq-fe.h>
#include <chrono>
#include <exception>


namespace asiopq {


	/**
	 *	Represents an abstract asynchronous libpq operation.
	 */
	class operation {


		public:


			/**
			 *	The type of a handle which represents a libpq
			 *	connection.
			 */
			using native_handle_type=PGconn *;
			/**
			 *	The type which represents the amount of time an
			 *	operation may take before timing out.
			 *
			 *	May be nulled in which case the operation is
			 *	permitted to take arbitrarily long.
			 */
			using timeout_type=optional<std::chrono::milliseconds>;


			operation () = default;
			operation (const operation &) = delete;
			operation (operation &&) = delete;
			operation & operator = (const operation &) = delete;
			operation & operator = (operation &&) = delete;


			/**
			 *	Allows derived classes to be cleaned up through
			 *	pointer or reference to base.
			 */
			virtual ~operation () noexcept;


			/**
			 *	An enumeration of socket statuses.
			 *
			 *	The members of this enumeration indicate which
			 *	operation may be performed on a socket without
			 *	blocking.
			 */
			enum class socket_status {

				/**
				 *	Indicates that the underlying libpq socket
				 *	can be read without blocking.
				 */
				readable,
				/**
				 *	Indicates that the underlying libpq socket
				 *	may be written without blocking.
				 */
				writable

			};


			/**
			 *	An enumeration of operation statuses.
			 *
			 *	The members of this enumeration indicate which
			 *	socket status the operation requires, or if the
			 *	operation has completed.
			 */
			enum class operation_status {

				/**
				 *	The operation has either succeeded or failed.
				 */
				done,
				/**
				 *	The operation can only continue once the underlying libpq
				 *	socket may be read without blocking.
				 */
				read,
				/**
				 *	The operation can only continue once the underlying libpq
				 *	socket may be written without blocking.
				 */
				write,
				/**
				 *	The operation can only continue once the underlying libpq
				 *	socket may be written or read without blocking.
				 */
				read_write

			};


			/**
			 *	Invoked when the operation completes.  An operation
			 *	completes when either:
			 *
			 *	-	It is aborted
			 *	-	\ref begin or \ref perform returns \ref operation_status::done.
			 *
			 *	If any of this operation's handlers threw an exception
			 *	this is passed through the \em ex parameter otherwise
			 *	that parameter is a null std::exception_ptr.
			 *
			 *	\param [in] ex
			 *		A std::exception_ptr representing an exception thrown
			 *		in the execution of this operation, if any.
			 */
			virtual void complete (std::exception_ptr ex) = 0;


			/**
			 *	Invoked when the operation begins.
			 *
			 *	\param [in] handle
			 *		The libpq handle upon which to operate.
			 *
			 *	\return
			 *		An \ref operation_status enumerator which
			 *		indicates the status of the operation.
			 */
			virtual operation_status begin (native_handle_type handle) = 0;


			/**
			 *	Invoked when the last reported \ref operation_status
			 *	is satisfied.
			 *
			 *	\param [in] handle
			 *		The libpq handle upon which to operate.
			 *	\param [in] status
			 *		The current socket status.
			 *
			 *	\return
			 *		An \ref operation_status enumerator which
			 *		indicates the status of the operation.
			 */
			virtual operation_status perform (native_handle_type handle, socket_status status) = 0;


			/**
			 *	Retrieves the timeout for this operation.
			 *
			 *	This is the total amount of time this operation
			 *	is permitted to take.  Should the operation take
			 *	longer than this then \ref complete will be invoked
			 *	with an error.
			 *
			 *	\return
			 *		The timeout for this operation.
			 */
			virtual timeout_type timeout () = 0;


	};


}
