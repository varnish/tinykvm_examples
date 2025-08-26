#include "../kvm_api.h"
#include <string.h>
#include <strings.h>
static struct kvm_request req;

void vd_wait_for_requests_paused()
{
	wait_for_requests_paused(&req);
}

const char *vd_request_method()
{
	return req.method;
}

const char *vd_request_url()
{
	return req.url;
}

const char *vd_request_argument()
{
	return req.arg;
}

const char *vd_request_header(const char *header)
{
	// Headers are zero-terminated single "key: value" strings
	for (unsigned i = 0; i < req.num_headers; i++) {
		// Split the header into key and value
		const char *key = req.headers[i].field;
		const char *value = strchr(key, ':');
		if (value == NULL) {
			continue;
		}
		// Check if the key matches the requested header
		if (strncasecmp(key, header, value - key) == 0) {
			// Return the entire header
			return key;
		}
	}
	return NULL;
}

void vd_response_str(int status, const char *content_type, const char *body)
{
	backend_response_str(status, content_type, body);
}

void dummy_response_str()
{
	backend_response_str(200, "text/plain", "Hello, world!");
}

char* vd_find_header(const char* name) {
	return (char*)http_alloc_find(REQ, name);
}

void vd_set_req_header(const char* full, size_t len) {
	sys_http_set(REQ, full, len);
}
void vd_set_resp_header(const char* full, size_t len) {
	sys_http_set(RESP, full, len);
}
