/**
 *	\file
 */


#pragma once


#include "asio.hpp"
#include "operation.hpp"
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <type_traits>


namespace asiopq {


	/**
	 *
	 */
	class connection {


		public:


			/**
			 *	The type of a handle which represents a libpq
			 *	connection.
			 */
			using native_handle_type=operation::native_handle_type;
			/**
			 *	A smart pointer to an \ref operation.
			 */
			using operation_type=std::shared_ptr<operation>;


		private:


			class control {


				private:


					using mutex_type=std::mutex;
					mutable std::mutex m_;
					bool stop_;
					connection * self_;


				public:


					asio::steady_timer timer;


					control (connection &, asio::io_service &);


					using guard_type=std::unique_lock<mutex_type>;
					guard_type lock () const noexcept;
					void stop () noexcept;
					explicit operator bool () const noexcept;
					void move (connection &) noexcept;
					connection & self () const noexcept;


			};


			native_handle_type handle_;
			asio::io_service & ios_;
			operation_type op_;
			std::deque<operation_type> pending_;
			asio::ip::tcp::socket socket_;
			std::shared_ptr<control> control_;
			bool read_;
			bool write_;
			#ifdef _WIN32
			std::uint32_t sid_;
			#endif


			void update_socket ();
			template <typename F>
			auto wrap (F &&) noexcept(std::is_nothrow_move_constructible<F>::value);
			void next ();
			void begin ();
			void dispatch (operation::operation_status);
			void perform (operation::socket_status);


		public:


			connection () = delete;
			connection (const connection &) = delete;
			connection & operator = (const connection &) = delete;
			connection & operator = (connection &&) = delete;


			/**
			 *	Creates a connection object by assuming ownership
			 *	of a libpq handle.
			 *
			 *	If this constructor fails ownership of the libpq
			 *	handle is not assumed.
			 *
			 *	\param [in] handle
			 *		The libpq handle which the newly-created object
			 *		shall assume ownership of.
			 *	\param [in] ios
			 *		A asio::io_service which the newly-created
			 *		connection shall use to dispatch asynchronous
			 *		operations.
			 */
			connection (native_handle_type handle, asio::io_service & ios);


			/**
			 *	Constructs a connection object by moving an existing
			 *	connection object.
			 *
			 *	After this constructor completes the only thing which may
			 *	be safely done with \em rhs is to allow its lifetime
			 *	to end.
			 *
			 *	\param [in] rhs
			 *		The connection object to move from.
			 */
			connection (connection && rhs) noexcept;


			/**
			 *	Aborts all pending operations and cleans up the
			 *	managed libpq handle.
			 */
			~connection () noexcept;


			/**
			 *	Enqueues an \ref operation to execute on the connection.
			 *
			 *	\ref operation object's enqueued to a connection execute in
			 *	FIFO order.
			 *
			 *	None of the \ref operation object's methods shall be invoked
			 *	within this function.  All shall be invoked on threads running
			 *	the connection object's associated asio::io_service.
			 *
			 *	\param [in] op
			 *		The \ref operation to execute on the connection.
			 */
			void add (operation_type op);


			/**
			 *	Retrieves the underlying asio::io_service.
			 *
			 *	\return
			 *		A reference to an asio::io_service.
			 */
			asio::io_service & get_io_service () const noexcept;


			/**
			 *	Retrieves the managed libpq handle.
			 *
			 *	This object maintains ownership of the returned
			 *	handle.
			 *
			 *	\return
			 *		A libpq handle.
			 */
			native_handle_type native_handle () const noexcept;


	};


}
