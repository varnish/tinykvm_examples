#include "kvm_api.hpp"

#include <malloc.h>
#include <string.h>
#define ZSTD_DISABLE_DEPRECATE_WARNINGS
#include "../zstd/lib/zstd.h"
#include <nlohmann/json.hpp>

template <typename T>
static void transform(T& j,
	std::string_view content_type, std::string_view content)
{
	const auto action = j["action"];

	if (j.contains("resp_headers"))
	{
		auto hdrs = j["resp_headers"].template get<std::vector<std::string>>();
		for (const auto& hdr : hdrs) {
			sys_http_append(RESP, hdr.c_str(), hdr.size());
		}
	}

	if (action == "decompress")
	{
		unsigned long long const rSize = ZSTD_getFrameContentSize(content.begin(), content.size());

		const int is_compressed = (rSize != ZSTD_CONTENTSIZE_ERROR && rSize != ZSTD_CONTENTSIZE_UNKNOWN);
		if (is_compressed)
		{
			char *rBuff = (char *)malloc(rSize);
			size_t const dSize = ZSTD_decompress(rBuff, rSize, content.begin(), content.size());

			if (ZSTD_isError(dSize)) {
				Http::append(RESP, "X-Error: true");
				// Let's not send original if the decompression fails
				Backend::response(500, "text/plain", "Failed to decompress asset");
			}
			else {
				Http::set(RESP, "Content-Encoding");

				// Decompressed, but unknown content type
				backend_response(200, "", 0, rBuff, rSize);
			}
		} else {
			// Uncompressed backend asset
			Backend::response(200, content_type, content);
		}
	}
	else if (action == "compress")
	{
		int level = 5;
		if (j.contains("level")) level = j["level"];

		const size_t cBound = ZSTD_compressBound(content.size());
		char *cBuff = (char *)malloc(cBound);
		size_t const cSize = ZSTD_compress(cBuff, cBound, content.begin(), content.size(), level);

		if (ZSTD_isError(cSize)) {
			Http::append(RESP, "X-Error: true");
			// Let's not send original if the decompression fails
			Backend::response(500, "text/plain", "Failed to compress asset");
		}
		else {
			Http::set(RESP, "Content-Encoding: zstd");
			// Send compressed asset
			backend_response(200, "", 0, cBuff, cSize);
		}
	}
	else
	{
		Backend::response(503, "text/plain", "Unknown action");
	}
}

static void
on_get(const char *url, const char* arg)
{
	// Parse JSON with comments enabled
	const auto j = nlohmann::json::parse(arg, arg + strlen(arg), nullptr, true, true);

    std::vector<std::string> headers;
	if (j.contains("headers")) {
		headers = j["headers"].get<std::vector<std::string>>();
    }

	// Fetch content using cURL
	struct curl_options options = {};
	options.follow_location = true;
	options.dont_verify_host = true;

	auto resp = Curl::fetch(url, headers, &options);
	if (resp.failed()) {
		backend_response_str(503, "text/plain", "Failed to retrieve asset from backend");
	}

	transform(j, resp.content_type, resp.content);
}

static void
on_post(const char *url, const char *arg, const char *ctype, const uint8_t *data, size_t len)
{
	// Parse JSON with comments enabled
	const auto j = nlohmann::json::parse(arg, arg + strlen(arg), nullptr, true, true);

	// Get Content-Type from current request
	transform(j, ctype, std::string_view{(const char *)data, len});
}

int main()
{
	set_backend_get(on_get);
	set_backend_post(on_post);
	wait_for_requests();
}
