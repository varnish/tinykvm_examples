#include "kvm_api.hpp"
#include <nlohmann/json.hpp>

static void on_get(const char *url, const char *arg)
{
	std::vector<std::string> headers;

	/* Optional JSON document */
	const auto arglen = strlen(arg);
	if (arglen > 0)
	{
		const auto j = nlohmann::json::parse(arg, arg + arglen, nullptr, true, true);

		if (j.contains("headers")) {
			headers = j["headers"].get<std::vector<std::string>>();
		}
	}

	struct curl_options options = {};
	options.follow_location = true;
	options.dont_verify_host = true;

	const auto res = Curl::fetch(url, headers, &options);
	Backend::response(res.status, res.content_type, res.content);
}

int main()
{
	set_backend_get(on_get);
	wait_for_requests();
}
