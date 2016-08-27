/**
 *	\file
 */


#pragma once


#include "asio.hpp"
#include "connection.hpp"
#include "future.hpp"
#include "operation.hpp"
#include <exception>
#include <memory>


namespace asiopq {


	/**
	 *	Represents the operation of connecting to a
	 *	Postgres database.
	 */
	class connect : public operation, public std::enable_shared_from_this<connect> {


		private:


			native_handle_type handle_;
			promise<void> promise_;
			timeout_type timeout_;


			void init ();


		public:


			connect () = delete;


			/**
			 *	Connects to a Postgres database by calling
			 *	PQconnectStartParams.
			 *
			 *	\param [in] keywords
			 *		See libpq documentation for PQconnectStartParams.
			 *	\param [in] values
			 *		See libpq documentation for PQconnectStartParams.
			 *	\param [in] expand_dbname
			 *		See libpq documentation for PQconnectStartParams.
			 *	\param [in] timeout
			 *		A \ref operation::timeout_type object giving the
			 *		amount of time this operation is permitted to
			 *		take at maximum.  Defaults to no timeout which
			 *		means this operation may take infinitely long.
			 */
			connect (const char * const * keywords, const char * const * values, int expand_dbname, timeout_type timeout=timeout_type{});
			/**
			 *	Connects to a Postgres database by calling
			 *	PQconnectStart.
			 *
			 *	\param [in] conninfo
			 *		See libpq documentation for PQconnectStart.
			 *	\param [in] timeout
			 *		A \ref operation::timeout_type object giving the
			 *		amount of time this operation is permitted to
			 *		take at maximum.  Defaults to no timeout which
			 *		means this operation may take infinitely long.
			 */
			explicit connect (const char * conninfo, timeout_type timeout=timeout_type{});


			/**
			 *	Cleans up this operation.
			 */
			~connect () noexcept;


			/**
			 *	Fetches a \ref asiopq::connection object and dispatches
			 *	this operation thereupon.
			 *
			 *	\param [in] ios
			 *		The asio::io_service which the created
			 *		\ref asiopq::connection shall use to dispatch
			 *		asynchronous operations.
			 *
			 *	\return
			 *		An \ref asiopq::connection object with this
			 *		object as the sole pending operation.
			 */
			asiopq::connection connection (asio::io_service & ios);


			virtual void complete (std::exception_ptr) override;
			virtual operation_status begin (native_handle_type) override;
			virtual operation_status perform (native_handle_type, socket_status) override;
			virtual timeout_type timeout () override;


			/**
			 *	Retrieves a future which shall complete when this
			 *	operation completes.
			 *
			 *	\return
			 *		A future.
			 */
			future<void> get_future ();


	};


}
