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

static char*  compression_buffer = NULL;
static size_t compression_buffer_size = 0;

static void on_post(const char* url, const char* arg,
	const char* ctype, const uint8_t* content, size_t content_length)
{
	// Let's only do compression now
	(void) url;
	(void) arg; // TODO: JSON document as argument
	const int level = 1; // 1 = fast, 9 = slow

	z_stream outstream;
	outstream.zalloc = Z_NULL;
	outstream.zfree = Z_NULL;
	outstream.opaque = Z_NULL;
	outstream.avail_in = (uInt)content_length;
	outstream.next_in = (Bytef *)content;

	// Allocate the output buffer using compressBound()
	const size_t needed = compressBound(content_length);
	if (compression_buffer_size < needed) {
		compression_buffer = realloc(compression_buffer, needed);
		if (compression_buffer == NULL) {
			backend_response_str(503, "text/plain", "Failed to allocate compression buffer");
			return;
		}
		compression_buffer_size = needed;
	}

	outstream.next_out = (Bytef *)compression_buffer;
	outstream.avail_out = compression_buffer_size;
	outstream.data_type = Z_BINARY;
	int res = deflateInit2(&outstream, level, Z_DEFLATED, 16 + MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
	if (res != Z_OK) {
		backend_response_str(503, "text/plain", "Failed to initialize compression");
		return;
	}
	res = deflate(&outstream, Z_FINISH);
	if (res != Z_STREAM_END) {
		deflateEnd(&outstream);
		backend_response_str(503, "text/plain", "Failed to compress data");
		return;
	}
	const size_t total = outstream.total_out;
	deflateEnd(&outstream);

	http_set_str(RESP, "Content-Encoding: gzip");
	http_set_str(RESP, "Vary: Accept-Encoding");
	backend_response(200, ctype, strlen(ctype), compression_buffer, total);
}

int main()
{
	// Create initial compression buffer
	const size_t initial_size = 4UL << 20; // 4MB
	compression_buffer = malloc(initial_size);
	if (compression_buffer == NULL) {
		fprintf(stderr, "Failed to allocate compression buffer\n");
		return 1;
	}
	compression_buffer_size = initial_size;

	set_backend_get(on_get);
	set_backend_post(on_post);
	wait_for_requests();
}
