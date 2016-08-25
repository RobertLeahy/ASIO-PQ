/**
 *	\file
 */


#pragma once


#include "operation.hpp"
#include <stdexcept>


namespace asiopq {


	/**
	 *	A base class for all ASIO PQ errors.
	 */
	class error : public std::runtime_error {


		public:


			using std::runtime_error::runtime_error;


	};


	/**
	 *	Represents an exception thrown by a libpq
	 *	connection.
	 */
	class connection_error : public error {


		public:


			/**
			 *	The type of a libpq connection.
			 */
			using native_handle_type=operation::native_handle_type;


			/**
			 *	Creates a connection_error object by drawing
			 *	error information from a libpq connection.
			 *
			 *	\param [in] handle
			 *		The handle from which to draw error information.
			 */
			explicit connection_error (native_handle_type handle);


	};


	/**
	 *	Indicates that an \ref operation was aborted.
	 */
	class aborted : public error {


		public:


			aborted ();


	};


	/**
	 *	Indicates that an \ref operation took longer than
	 *	allowed.
	 */
	class timed_out : public error {


		private:


			operation::timeout_type::value_type timeout_;


		public:


			/**
			 *	Creates a new timed_out object.
			 *
			 *	\param [in] timeout
			 *		The duration of the timeout which elapsed.
			 */
			explicit timed_out (operation::timeout_type::value_type timeout);


			/**
			 *	Retrieves the duration of the elapsed timeout.
			 *
			 *	\return
			 *		A duration.
			 */
			operation::timeout_type::value_type timeout () const noexcept;


	};


}
