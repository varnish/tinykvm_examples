#include "../kvm_api.h"
#include <string.h>

static void
on_get(const char *url, const char *arg)
{
	http_append_str(RESP, "X-Hello: World");

	/* Fill out some request header fields. */
	struct curl_fields fields = {};
	fields.ptr[0] = "X-Hello: World";
	fields.len[0] = 14;

	/* Retrieve /headers from local HTTPBIN server. */
	struct curl_op op = {};
	const char* curl_url = "http://127.0.0.1:7070/headers";
	sys_fetch(curl_url, strlen(curl_url), &op, &fields, NULL);

	/* Add the Server: field to our own response. */
	char *srv = strstr(op.headers, "Server: ");
	*strchr(srv, '\r') = 0;
	http_append_str(RESP, srv);

	/* Cache 200s for 10 seconds. */
	set_cacheable(op.status == 200, 10.0f, 0.0f, 0.0f);

	/* Respond with the example.com content. */
	backend_response(200, op.ctype, op.ctlen, op.content, op.content_length);
}

int main(int argc, char **argv)
{
	set_backend_get(on_get);
	wait_for_requests();
}
