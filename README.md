# ASIO PQ [![Build Status](https://travis-ci.org/RobertLeahy/ASIO-PQ.svg?branch=master)](https://travis-ci.org/RobertLeahy/ASIO-PQ)

ASIO cURL is a C++14 library which provides asynchronous access to the facilities of [libpq](https://www.postgresql.org/docs/current/static/libpq.html) using [ASIO](http://think-async.com/) or [Boost.ASIO](http://www.boost.org/doc/libs/1_61_0/doc/html/boost_asio.html).

## What is ASIO PQ?

ASIO PQ is not a C++ wrapper for libpq.  When using libpq you will still interact with `PGconn *` (usually referred to as `asiopq::connection::native_handle_type`) and `PGresult *` (usually referred to as `asiopq::query::native_result_type`) objects.  This is a deliberate choice: The full power and flexibility of libpq remains available to you (nothing is long in translation or abstraction) while allowing you to get a modern, C++14, asynchronous experience.

## How does ASIO PQ Work?

Use of ASIO PQ revolves around two classes:

- `asiopq::connection`
- `asiopq::operation`

An `asiopq::connection` object wraps a `PGconn *` and uses a `boost::asio::io_service` or `asio::io_service` to dispatch asynchronous operations.

`asiopq::operation` is an abstract base class with no data members and only pure virtual function members (i.e. an interface) which represents an asynchronous operation which an `asiopq::connection` may perform.

All libpq asynchronous operations are divided into three phases:

1. `asiopq::operation::begin` is invoked when the `asiopq::connection` is ready to begin an operation
2. `asiopq::operation::perform` is invoked whenever the socket associated with the `PGconn *` the operation is running on becomes available
3. `asiopq::operation::complete` is invoked when the operation completes, times out, or fails

To perform an operation against the database you implement a class which derives from `asiopq::operation`, implement these three phases using raw, C-style calls to libpq, pass instances of your operation class to `asiopq::connection::add`, and then make sure there are threads running the associated `boost::asio::io_service` or `asio::io_service`.

## Convenience Classes

The two classes mentioned in the preceding section are all you need to know about and use to take advantage of ASIO PQ.  However ASIO PQ includes several classes which can save you considerable development (and save you a lot of interaction with the libpq C API):

- `asiopq::connect` represents an asynchronous connect attempt dispatched using either `PQconnectStart` or `PQconnectStartParams` (which function is used depends on your choice of constructor)
- `asiopq::reset` represents an asynchronous reset attempt dispatched using `PQresetStart`
- `asiopq::query` reduces the act of querying the database to simply deriving from it and implementing:
	- `asiopq::query::start` which submits the query asynchronously
	- `asiopq::query::result` which is passed each `PGresult *`
	- `asiopq::operation::complete` invoked when there are no more `PGresult *`, or when the operation fails or times out

ASIO PQ also includes exception types designed to make interoperating with the libpq library (specifically error handling) simpler:

- `asiopq::connection_error` accepts a `PGconn *` and sets its error message appropriately for the last error which occurred on the connection
- `asiopq::result_error` accepts a `PGresult *` and sets its error message appropriately for the last error which occurred on the result

## Boost

ASIO PQ can use Boost for `boost::future` and `boost::asio`, or it can use `asio` and `std::future`.  The choice is yours to make when you build the library.

Pass CMake `USE_BOOST_FUTURE=0` to use `std::future` and `USE_BOOST_ASIO=0` to use non-Boost ASIO.  Both of these default to 1 (i.e. use Boost.Future & Boost.ASIO).

If you set them both to zero Boost is not a dependency at all.  Several CI builds are performed in environments where Boost is not installed at all to verify this.

Note that if you set `USE_BOOST_ASIO=0` you will be expected to provide ASIO.

## Dependencies

- Boost (see above)
- ASIO (see above)
- libpq

Due to the way `FindPostgreSQL.cmake` works you may need to install `postgres-server-dev-9.3` on Ubuntu (CMake can't find `pg_types.h` without it despite the fact the project doesn't use `pg_types.h`).

## Supported Compilers

The following compilers have been tested:

### Linux

- Clang 3.8
- GCC 5.3.0
- GCC 6.1.1

### Windows

- GCC 5.3.0

## Build

#### Linux

```
cmake .
make
```

### Windows

```
cmake -G "MinGW Makefiles" .
make
```

To build the tests call CMake with `BUILD_TESTS=1`.  Note that this adds [Catch](https://github.com/philsquared/Catch) as a dependency and you will be expected to have a PostgreSQL server that can be accessed for integration testing.  If you want to know more about this examine `src/test/login.hpp.in`.

## Documentation

To build full documentation simply run [`doxygen`](http://www.stack.nl/~dimitri/doxygen/).