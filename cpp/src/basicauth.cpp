#include "kvm_api.hpp"
#include <string_view>
#include <unordered_set>
#include <turbob64.h>
static std::unordered_set<std::string> authenticated;

static void authenticate(size_t n, struct virtbuffer buffer[n], size_t res)
{
	std::string password((const char *)buffer[0].data, buffer[0].len);
	std::string inpass((const char *)buffer[1].data, buffer[1].len);
	if (!inpass.empty()) // Remove 'Authorization: '
		inpass = inpass.substr(15);

	std::string candidate((const char *)buffer[2].data, buffer[2].len);

	if (authenticated.count(candidate))
	{
		storage_return("OK", 2);
	}
	else if (password == inpass)
	{
		authenticated.insert(candidate);
		storage_return("OK", 2);
	}
	else {
		storage_return("Failed", 6);
	}
}

std::string to_base64(std::string_view text)
{
	const size_t in_len  = text.size();
	const size_t out_len = tb64enclen(in_len);
	std::string result;
	result.resize(out_len);
	tb64enc((const uint8_t *)text.begin(), in_len, (uint8_t *)result.data());
	return result;
}

static std::string authenticate(
	std::string_view password,
	const char *info)
{
	std::vector<std::string> inputs;
	inputs.push_back("Basic " + to_base64(password));
	inputs.push_back(Http::get(REQ, "Authorization"));
	inputs.push_back(info);
	return Storage::get(inputs, authenticate);
}

static void
on_get(const char *password, const char *info)
{
	auto result = authenticate(password, info);

	if (result == "OK") {
		Backend::response(200, "text/plain", "OK");
	} else {
		//Http::set(HDR_REQ, "WWW-Authenticate: Basic realm=\"Access to section\"");
		Backend::response(401, "text/plain", "");
	}
}

static void
on_post(const char *password, const char *info,
	const char *content_type, const uint8_t *data, size_t len)
{
	auto result = authenticate(password, info);

	if (result == "OK") {
		Backend::response(200, content_type, data, len);
	} else {
		Backend::response(401, "text/plain", "Access denied");
	}
}

static void
on_error(const char *url, const char *, const char *)
{
	//Http::set(HDR_REQ, "WWW-Authenticate: Basic realm=\"Access to section\"");
	Backend::response(401, "text/plain", "");
}

int main(int argc, char **argv)
{
	set_backend_get(on_get);
	set_backend_post(on_post);
	set_on_error(on_error);
	wait_for_requests();
}
