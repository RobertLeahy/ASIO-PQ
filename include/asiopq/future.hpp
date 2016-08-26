/**
 *	\file
 */


#pragma once


#include "configure.hpp"
#include <exception>
#include <utility>


#ifdef ASIOPQ_USE_BOOST_FUTURE
#ifndef BOOST_THREAD_PROVIDES_FUTURE
#define BOOST_THREAD_PROVIDES_FUTURE
#endif
#include <boost/exception_ptr.hpp>
#include <boost/thread/future.hpp>
#else
#include <future>
#endif


namespace asiopq {


	#ifdef ASIOPQ_USE_BOOST_FUTURE

	using boost::future;
	using boost::promise;


	template <typename T>
	void set_exception (promise<T> & promise, std::exception_ptr ex) noexcept {

		try {

			std::rethrow_exception(std::move(ex));

		} catch (...) {

			promise.set_exception(boost::current_exception());

		}

	}

	#else

	using std::future;
	using std::promise;


	template <typename T>
	void set_exception (promise<T> & promise, std::exception_ptr ex) noexcept {

		promise.set_exception(std::move(ex));

	}

	#endif


}
