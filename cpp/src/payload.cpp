#include "kvm_api.hpp"
#include <cstring>
#include <nlohmann/json.hpp>
#include <random>
#include <string>
// Cached payload
static std::string payload;
static std::random_device rd {};
static std::mt19937 gen {rd()};
static std::uniform_int_distribution<> dis(0, 255);

static void on_get(const char *url, const char *arg)
{
	size_t payload_size = 0;
	bool randomize = false;

	/* Optional JSON document */
	const auto arglen = strlen(arg);
	if (arglen > 0)
	{
		const auto j = nlohmann::json::parse(arg, arg + arglen, nullptr, true, true);

		if (j.contains("size")) {
			payload_size = j["size"].get<size_t>();
		}
		if (j.contains("randomize")) {
			randomize = j["randomize"].get<bool>();
		}
	}

	if (payload.size() != payload_size)
	{
		payload.resize(payload_size);
		if (randomize) {
			// Fill with pseudo-random bytes
			for (size_t i = 0; i < payload_size; ++i) {
				payload[i] = static_cast<char>(dis(gen));
			}
		} else {
			// Fill with 'a' characters
			memset(payload.data(), 'a', payload_size);
		}
	}

	Backend::response(200, url, payload.c_str(), payload.size());
}

int main()
{
	set_backend_get(on_get);
	wait_for_requests();
}
