#include "kvm_api.hpp"
#include <nlohmann/json.hpp>
#include <SQLiteCpp/SQLiteCpp.h>
static std::unique_ptr<SQLite::Database> db;

static void
on_post(const char *url, const char *arg,
	const uint8_t* data, size_t data_size)
{
	try
	{
		// Compile a SQL query, containing one parameter (index 1)
		SQLite::Statement   query(*db, "SELECT * FROM test WHERE size > 6");

		query.exec();

		// Loop to execute the query step by step, to get rows of result
		while (query.executeStep())
		{
			// Demonstrate how to get some typed column value
			int         id      = query.getColumn(0);
			const char* value   = query.getColumn(1);
			int         size    = query.getColumn(2);

			std::cout << "row: " << id << ", " << value << ", " << size << std::endl;
		}
	}
	catch (std::exception& e)
	{
		Backend::response(503, "text/plain", "Database operation failed");
	}
	/* Cache for 10 seconds. */
	set_cacheable(false, 10.0f, 0.0f, 0.0f);

	/* Respond with the image. */
	Backend::response(resp.status, resp.content_type, resp.content);
}

int main()
{
	db = new SQLite::Database("state");

	set_backend_post(on_post);
	wait_for_requests();
}
