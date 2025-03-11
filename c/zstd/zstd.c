#define _GNU_SOURCE
#include "kvm_api.h"

#include <malloc.h>
#include <string.h>
#include "zstd/lib/zstd.h"

static void on_get(const char *url, const char* arg)
{
	// TODO: Check if content is already compressed:
	// "Encoding: gzip" (or deflate)
	struct curl_op op = {};
	struct curl_fields fields = {};
	CURL_SETFIELD(fields, 0, "Accept-Encoding: gzip");

	if (sys_fetch(url, __builtin_strlen(url), &op, &fields, NULL) != 0)
	{
		backend_response_str(503, "text/plain", "Failed to retrieve asset from backend");
	}

	if (arg != NULL)
		http_appendf(RESP, "Content-Disposition: %s", arg);

	unsigned long long const rSize = ZSTD_getFrameContentSize(op.content, op.content_length);

	const int is_compressed = (rSize != ZSTD_CONTENTSIZE_ERROR && rSize != ZSTD_CONTENTSIZE_UNKNOWN);
	if (is_compressed)
	{
		char *rBuff = (char *)malloc(rSize);
		size_t const dSize = ZSTD_decompress(rBuff, rSize, op.content, op.content_length);

		if (ZSTD_isError(dSize)) {
			http_set_str(RESP, "Content-Encoding: zstd");
			backend_response(200, op.ctype, op.ctlen, op.content, op.content_length);
		}
		else {
			// Send decompressed shit (NOTE: dSize == rSize)
			backend_response(200, op.ctype, 0, rBuff, rSize);
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
