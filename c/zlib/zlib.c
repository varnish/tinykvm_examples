#define _GNU_SOURCE
#include "kvm_api.h"
#include <malloc.h>
#include <string.h>
#include <zlib.h>

static void on_get(const char *url, const char *arg)
{
	(void) arg;

	struct curl_op op = {};
	op.headers_length = 16384; // We want response headers
	struct curl_fields fields = {};
	CURL_SETFIELD(fields, 0, "Accept-Encoding: gzip");

	if (sys_fetch(url, __builtin_strlen(url), &op, &fields, NULL) != 0)
	{
		backend_response_str(503, "text/plain", "Failed to retrieve asset from backend");
	}

	const int is_compressed = op.headers != NULL && strcasestr(op.headers, "Content-Encoding: gzip") != NULL;
	if (is_compressed)
	{
		const size_t MAX_DECOMPR = 512UL << 20;
		uint8_t* cont = malloc(MAX_DECOMPR); // 512MB buffer

		z_stream infstream;
		infstream.zalloc = Z_NULL;
		infstream.zfree = Z_NULL;
		infstream.opaque = Z_NULL;
		infstream.avail_in = (uInt)op.content_length;
		infstream.next_in = (Bytef *)op.content;
		infstream.avail_out = MAX_DECOMPR;
		infstream.next_out = (Bytef *)cont;

		inflateInit2(&infstream, 16 + MAX_WBITS);
		int res = inflate(&infstream, Z_NO_FLUSH);
		size_t total = infstream.total_out;
		//inflateEnd(&infstream);

		if (res > 0) {
			// Send decompressed shit
			backend_response(200, op.ctype, op.ctlen, cont, total);
		}
		else {
			// Send original (XXX: Add encoding?)
			backend_response(200, op.ctype, op.ctlen, op.content, op.content_length);
		}
	} else {
		// Uncompressed backend asset
		backend_response(200, op.ctype, op.ctlen, op.content, op.content_length);
	}
}

int main()
{
	set_backend_get(on_get);
	wait_for_requests();
}
