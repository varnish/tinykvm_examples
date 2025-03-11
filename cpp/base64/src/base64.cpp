#include "kvm_api.hpp"

#include <cstring>
#include <nlohmann/json.hpp>
#include <turbob64.h>

static void
on_request(const struct backend_request *req)
{
	//const auto j = nlohmann::json::parse(req->arg, req->arg_len, nullptr, true, true);
	const uint8_t *in = nullptr;
	size_t in_len = 0;
	if (req->content_len > 0) {
		in     = req->content;
		in_len = req->content_len;
	} else {
		in     = (const uint8_t *)req->url;
		in_len = req->url_len;
	}

	const size_t out_len = tb64enclen(in_len);
	uint8_t *out = new uint8_t[out_len + 1];

	const size_t len = tb64enc(in, in_len, out);
	out[len] = 0;

	Backend::response(200, "text/plain", out, out_len);
}

int main()
{
	set_backend_request(on_request);
	wait_for_requests();
}
