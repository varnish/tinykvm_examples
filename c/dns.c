#include "../kvm_api.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int dns;

static void
on_get(const char *url, const char *arg)
{
	/* Get ADNS-managed DNS address at index 0. */
	struct adns adns;
	adns_get(dns, 0, &adns);

	/* Fetch from vg.no server using regular HTTP. */
	char curl_url[512];
	const int curl_len = snprintf(curl_url, sizeof(curl_url),
		"http://%s", adns.address);

	struct curl_options options = {};
	options.follow_location = 1;

	struct curl_op op = {};
	sys_fetch(curl_url, curl_len, &op, NULL, &options);

	/* Cache 200s for 10 seconds. */
	set_cacheable(op.status == 200, 10.0f, 0.0f, 0.0f);

	/* Respond with the example.com content. */
	backend_response(op.status, op.ctype, op.ctlen, op.content, op.content_length);
}

int main(int argc, char **argv)
{
	dns = adns_new(1234);
	const union adns_rules rules = {
		.b = {
			.ipv = ADNS_IPV_AUTO,
			.ttl = ADNS_TTL_ABIDE,
			.port = ADNS_PORT_ABIDE,
			.mode = ADNS_MODE_DNS,
			.update = ADNS_UPDATE_ALWAYS,
			.nsswitch = ADNS_NSSWITCH_AUTO,
		}
	};
	adns_config(dns, "www.vg.no", "http", 60.0, &rules);

	set_backend_get(on_get);
	wait_for_requests();
}
