/**
 * Copyright (c) 2025 Varnish Software AS
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
**/
#define KVM_API_ALREADY_DEFINED
#include "api.h"
#include <string.h>
#include <quickjs/cutils.h>
#include <quickjs/quickjs.h>
#include <quickjs/quickjs-libc.h>

static JSValue throw_error(JSContext *ctx, const char *reason) {
	return JS_Throw(ctx, JS_NewString(ctx, reason));
}

static JSValue get_headers(JSContext *ctx, const struct curl_op *op)
{
	JSValue array = JS_NewArray(ctx);
	uint32_t idx = 0;
	const char *hdr = op->headers;
	const char *hdr_end = hdr + op->headers_length;
	while (hdr < hdr_end) {
		const char *n = strchr(hdr, '\r');
		if (n == NULL) break;
		JS_DefinePropertyValueUint32(ctx, array,
			idx++, JS_NewStringLen(ctx, hdr, n - hdr), JS_PROP_C_W_E);
		hdr = n + 2;
	}
	return array;
}

static int get_request_fields(JSContext *ctx, struct curl_fields *fields,
	JSValueConst array)
{
	if (JS_IsArray(ctx, array)) {
		for (uint32_t i = 0; i < CURL_FIELDS_COUNT; i++) {
			JSValue val = JS_GetPropertyUint32(ctx, array, i);
			if (JS_IsException(val))
				break;

			size_t flen = 0;
			fields->ptr[i] = JS_ToCStringLen(ctx, &flen, val);
			fields->len[i] = flen;
		}
		return 0;
	} else {
		throw_error(ctx, "fetch: Second argument must be array");
		return -1;
	}
}

JSValue js_http_fetch_call(JSContext *ctx,
	JSValueConst this_val, int argc, JSValueConst *argv)
{
	(void)this_val;
	if (argc >= 1) {
		size_t urllen;
		const char* url =
			JS_ToCStringLen(ctx, &urllen, argv[0]);
		if (!url)
			return JS_EXCEPTION;
		/* Request headers */
		struct curl_fields fields = {};
		if (argc >= 2) {
			if (get_request_fields(ctx, &fields, argv[1]) < 0) {
				vlogf("qjs: fetch(%.*s) request headers failed", (int)urllen, url);
				return JS_EXCEPTION;
			}
		}
		/* Fetch whole thing */
		struct curl_op op = {};
		if (sys_fetch(url, urllen, &op, &fields, NULL) == 0) {
			JSValue result = JS_NewObject(ctx);
			JS_SetPropertyStr(ctx, result,
				"status",
				JS_NewUint32(ctx, op.status));
			JS_SetPropertyStr(ctx, result,
				"type",
				JS_NewStringLen(ctx, op.ctype, op.ctlen));
			JS_SetPropertyStr(ctx, result,
				"content",
				JS_NewStringLen(ctx, op.content, op.content_length));
			/* Response headers */
			JSValue array = get_headers(ctx, &op);
			JS_SetPropertyStr(ctx, result, "headers", array);
			return result;
		}
		vlogf("qjs: fetch(%.*s) failed", (int)urllen, url);
		return throw_error(ctx, "fetch() failed");
	}
	vlogf("qjs: fetch() not enough arguments");
	return throw_error(ctx, "Not enough arguments to fetch()");
}

JSValue js_http_post_call(JSContext *ctx,
	JSValueConst this_val, int argc, JSValueConst *argv)
{
	(void)this_val;
	if (argc >= 3)
	{
		size_t urllen;
		const char *url =
			JS_ToCStringLen(ctx, &urllen, argv[0]);
		if (!url)
			return JS_EXCEPTION;

		/* HTTP POST using curl.fetch dyncall */
		struct curl_op op = {};

		/* POST Content-Type */
		size_t ctype_len = 0;
		const char *ctype =
			JS_ToCStringLen(ctx, &ctype_len, argv[1]);
		if (!ctype)
			return JS_EXCEPTION;
		op.ctlen = ctype_len;
		memcpy(op.ctype, ctype, ctype_len);

		/* POST request headers */
		struct curl_fields fields = {};
		if (argc >= 4) {
			if (get_request_fields(ctx, &fields, argv[3]) < 0) {
				vlogf("qjs: post(%.*s) request headers failed", (int)urllen, url);
				return JS_EXCEPTION;
			}
		}

		/* POST data */
		size_t post_buflen;
		op.post_buffer =
			JS_ToCStringLen(ctx, &post_buflen, argv[2]);
		if (!op.post_buffer)
			return JS_EXCEPTION;
		op.post_buflen = post_buflen;

		if (sys_fetch(url, urllen, &op, &fields, NULL) == 0)
		{
			JSValue result = JS_NewObject(ctx);
			JS_SetPropertyStr(ctx, result,
				"status",
				JS_NewUint32(ctx, op.status));
			JS_SetPropertyStr(ctx, result,
				"type",
				JS_NewStringLen(ctx, op.ctype, op.ctlen));
			JS_SetPropertyStr(ctx, result,
				"content",
				JS_NewStringLen(ctx, op.content, op.content_length));
			/* Response headers */
			JSValue array = get_headers(ctx, &op);
			JS_SetPropertyStr(ctx, result, "headers", array);
			return result;
		}
		vlogf("qjs: post(%.*s) failed", (int)urllen, url);
		return throw_error(ctx, "post() failed");
	}
	vlogf("qjs: post() not enough arguments");
	return throw_error(ctx, "Not enough arguments to post()");
}
