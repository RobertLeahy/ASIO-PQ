#include <asiopq/asio.hpp>
#include <asiopq/connect.hpp>
#include <asiopq/exception.hpp>
#include <asiopq/future.hpp>
#include <asiopq/query.hpp>
#include <asiopq/reset.hpp>


#include "login.hpp"


#include <asiopq/scope.hpp>
#include <libpq-fe.h>
#include <chrono>
#include <exception>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <catch.hpp>


namespace {


	class no_result_query : public asiopq::query {


		private:


			asiopq::promise<void> promise_;



		public:


			using asiopq::query::query;


			virtual void result (native_result_type result) override {

				auto g=asiopq::make_scope_exit([&] () noexcept {	PQclear(result);	});
				if (PQresultStatus(result)!=PGRES_COMMAND_OK) throw asiopq::result_error(result);
				promise_.set_value();

			}


			virtual void complete (std::exception_ptr ex) override {

				if (ex) promise_.set_exception(std::move(ex));

			}


			asiopq::future<void> get_future () {

				return promise_.get_future();

			}


	};


	class create_query : public no_result_query {


		public:


			using no_result_query::no_result_query;


			virtual void send (native_handle_type handle) override {

				if (PQsendQuery(
					handle,
					"CREATE TABLE \"test\" (\"foo\" int);"
				)==0) throw asiopq::connection_error(handle);

			}


	};


	class insert_query : public no_result_query {


		private:


			int i_;


		public:


			insert_query (int i, timeout_type timeout=timeout_type{}) : no_result_query(timeout), i_(i) {	}


			virtual void send (native_handle_type handle) override {

				std::ostringstream ss;
				ss << "INSERT INTO \"test\" (\"foo\") VALUES (" << i_ << ");";
				auto str=ss.str();
				if (PQsendQuery(handle,str.c_str())==0) throw asiopq::connection_error(handle);

			}


	};


	class integer_query : public asiopq::query {


		private:


			asiopq::promise<int> promise_;


		public:


			using asiopq::query::query;


			virtual void result (native_result_type result) override {

				auto g=asiopq::make_scope_exit([&] () noexcept {	PQclear(result);	});
				if (PQresultStatus(result)!=PGRES_TUPLES_OK) throw asiopq::result_error(result);

				auto n=PQntuples(result);
				if (n!=1) {

					std::ostringstream ss;
					ss << "Expected 1 tuple, got " << n;
					throw std::runtime_error(ss.str());

				}

				auto cn=PQnfields(result);
				if (cn!=1) {

					std::ostringstream ss;
					ss << "Expected 1 field, got " << cn;
					throw std::runtime_error(ss.str());

				}

				promise_.set_value(std::stoi(PQgetvalue(result,0,0)));

			}


			virtual void complete (std::exception_ptr ex) override {

				if (ex) promise_.set_exception(std::move(ex));

			}


			asiopq::future<int> get_future () {

				return promise_.get_future();

			}


	};


	class count_query : public integer_query {


		public:


			using integer_query::integer_query;


			virtual void send (native_handle_type handle) override {

				if (PQsendQuery(
					handle,
					"SELECT COUNT(*) FROM \"test\";"
				)==0) throw asiopq::connection_error(handle);

			}


	};


	class min_query : public integer_query {


		public:


			using integer_query::integer_query;


			virtual void send (native_handle_type handle) override {

				if (PQsendQuery(
					handle,
					"SELECT MIN(\"foo\") FROM \"test\";"
				)==0) throw asiopq::connection_error(handle);

			}


	};


}


SCENARIO("ASIO PQ features may be used to connect to a PostgreSQL database, reset that connection, and submit queries","[asiopq][integration][connect][connection][query][reset]") {

	GIVEN("An asiopq::connect object representing a connection attempt to a PostgreSQL server") {

		asiopq::asio::io_service ios;
		const char * keywords []={
			"hostaddr",
			"port",
			"dbname",
			"user",
			"password",
			nullptr
		};
		const char * values []={
			ASIOPQ_HOST_ADDR,
			ASIOPQ_PORT,
			ASIOPQ_DATABASE_NAME,
			ASIOPQ_USERNAME,
			ASIOPQ_PASSWORD,
			nullptr
		};
		std::chrono::milliseconds timeout(1000);
		auto connect=std::make_shared<asiopq::connect>(keywords,values,0,timeout);
		auto connection=connect->connection(ios);

		WHEN("That connection attempt is run on an asiopq::connection") {

			ios.run();
			
			THEN("The connection completes successfully") {

				CHECK_NOTHROW(connect->get_future().get());

				GIVEN("An asiopq::reset object") {

					auto reset=std::make_shared<asiopq::reset>(timeout);

					WHEN("That reset attempt is run on the connected asiopq::connection") {

						connection.add(reset);
						ios.reset();
						ios.run();

						THEN("The reset completes successfully") {

							CHECK_NOTHROW(reset->get_future().get());

							GIVEN("Several queries represented by objects which derive from asiopq::query") {

								auto create=std::make_shared<create_query>(timeout);
								auto insert_1=std::make_shared<insert_query>(1,timeout);
								auto insert_2=std::make_shared<insert_query>(2,timeout);
								auto count=std::make_shared<count_query>(timeout);
								auto min=std::make_shared<min_query>(timeout);

								WHEN("They are run on the asiopq::connection") {

									connection.add(create);
									connection.add(insert_1);
									connection.add(insert_2);
									connection.add(count);
									connection.add(min);
									ios.reset();
									ios.run();

									THEN("They all complete successfully") {

										CHECK_NOTHROW(create->get_future().get());
										CHECK_NOTHROW(insert_1->get_future().get());
										CHECK_NOTHROW(insert_2->get_future().get());
										CHECK(count->get_future().get()==2);
										CHECK(min->get_future().get()==1);

									}

								}

							}

						}

					}

				}

			}

		}

	}

}
