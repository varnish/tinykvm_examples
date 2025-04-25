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
#include "api.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <quickjs/cutils.h>
#include <quickjs/quickjs.h>
#include <quickjs/quickjs-libc.h>
#define COUNTOF(a)  (sizeof(a) / sizeof(*(a)))
void dlopen() {}; void dlsym() {}; void dlclose() {} /* Don't ask */
static JSValue js_cacheable_call(JSContext *, JSValueConst, int, JSValueConst *);
static JSValue js_backend_response(JSContext *, JSValueConst, int, JSValueConst *);
static JSValue js_get_call(JSContext *, JSValueConst, int, JSValueConst *);
static JSValue js_set_call(JSContext *, JSValueConst, int, JSValueConst *);
static JSValue js_storage_call(JSContext *, JSValueConst, int, JSValueConst *);
static JSValue js_sendfile_call(JSContext*, JSValueConst, int, JSValueConst*);
extern JSValue js_http_fetch_call(JSContext *, JSValueConst, int, JSValueConst *);
extern JSValue js_http_post_call(JSContext *, JSValueConst, int, JSValueConst *);
extern void static_site(const char *);
static const int MAX_STORAGE_ARGS = 12;

static int eval_buf(JSContext *ctx, const void *buf, int buf_len,
					const char *filename, int eval_flags)
{
	JSValue val;
	int ret;

	if ((eval_flags & JS_EVAL_TYPE_MASK) == JS_EVAL_TYPE_MODULE) {
		/* for the modules, we compile then run to be able to set
		   import.meta */
		val = JS_Eval(ctx, buf, buf_len, filename,
					  eval_flags | JS_EVAL_FLAG_COMPILE_ONLY);
		if (!JS_IsException(val)) {
			js_module_set_import_meta(ctx, val, FALSE, TRUE);
			val = JS_EvalFunction(ctx, val);
		}
	} else {
		val = JS_Eval(ctx, buf, buf_len, filename, eval_flags);
	}
	if (JS_IsException(val)) {
		js_std_dump_error(ctx);
		ret = -1;
	} else {
		ret = 0;
	}
	JS_FreeValue(ctx, val);
	return ret;
}

#include "current_source.h"

static JSValue global_obj;
static JSContext *g_ctx = NULL;
static struct {
	JSValue varnish;
	JSValue backend_func;
	JSValue post_backend_func;
	JSValue error_func;
} vapi;

static JSValue throw_error(JSContext *ctx, const char *reason) {
	return JS_Throw(ctx, JS_NewString(ctx, reason));
}

JSValue js_backend_response(JSContext *ctx,
	JSValueConst this_val, int argc, JSValueConst *argv)
{
	(void)this_val;
	if (argc == 3) {
		int code;
		if (JS_ToInt32(ctx, &code, argv[0]))
			return JS_EXCEPTION;
		size_t tlen;
		const char* type =
			JS_ToCStringLen(ctx, &tlen, argv[1]);
		size_t clen;
		const char* cont =
			JS_ToCStringLen(ctx, &clen, argv[2]);
		if (!type || !cont)
			return JS_EXCEPTION;
		/* Give response to waiting clients */
		backend_response(code, type, tlen, cont, clen);
	}
	return JS_EXCEPTION;
}

static JSValue js_log_call(JSContext *ctx,
	JSValueConst this_val, int argc, JSValueConst *argv)
{
	(void)this_val;
	if (argc >= 1) {
		size_t tlen;
		const char* text =
			JS_ToCStringLen(ctx, &tlen, argv[0]);
		sys_log(text, tlen);
		return JS_TRUE;
	}
	return JS_EXCEPTION;
}

static JSValue js_cacheable_call(JSContext *ctx,
	JSValueConst this_val, int argc, JSValueConst *argv)
{
	(void)this_val;
	if (argc == 4) {
		int cached = JS_ToBool(ctx, argv[0]);
		double ttl;
		double grace;
		double keep;
		if (JS_ToFloat64(ctx, &ttl, argv[1]))
			return JS_EXCEPTION;
		if (JS_ToFloat64(ctx, &grace, argv[2]))
			return JS_EXCEPTION;
		if (JS_ToFloat64(ctx, &keep, argv[3]))
			return JS_EXCEPTION;
		set_cacheable(cached, ttl, grace, keep);
		return JS_TRUE;
	}
	return JS_EXCEPTION;
}

JSValue js_get_call(JSContext *ctx,
	JSValueConst this_val, int argc, JSValueConst *argv)
{
	(void)this_val;
	if (argc == 1) {
		size_t nlen;
		const char* name =
			JS_ToCStringLen(ctx, &nlen, argv[0]);
		if (!name)
			return JS_EXCEPTION;
		/* Create a JS string from the result */
		char buffer[1024];
		long len = sys_http_find(4, name, nlen, buffer, sizeof(buffer));
		if (len > 0) {
			return JS_NewStringLen(ctx, buffer, len);
		}
		JS_FreeCString(ctx, name);
		return throw_error(ctx, "HTTP header field could not be found");
	}
	return JS_EXCEPTION;
}

JSValue js_set_call(JSContext *ctx,
	JSValueConst this_val, int argc, JSValueConst *argv)
{
	(void)this_val;
	if (argc == 1) {
		size_t flen;
		const char* field =
			JS_ToCStringLen(ctx, &flen, argv[0]);
		if (!field)
			return JS_EXCEPTION;
		if (sys_http_set(5, field, flen) > 0)
			return JS_TRUE;
		JS_FreeCString(ctx, field);
		return throw_error(ctx, "HTTP header field could not be set");
	}
	return JS_EXCEPTION;
}

JSValue js_sendfile_call(JSContext *ctx,
	JSValueConst this_val, int argc, JSValueConst *argv)
{
	(void)this_val;
	if (argc == 1) {
		size_t flen;
		const char* filename =
			JS_ToCStringLen(ctx, &flen, argv[0]);
		if (!filename)
			return JS_EXCEPTION;
		/* Send static resource (or 404) */
		static_site(filename);
	}
	return JS_EXCEPTION;
}

static void
on_error(const char *url, const char *arg, const char *exception)
{
	JSValueConst argv[3];
	argv[0] = JS_NewString(g_ctx, url);
	argv[1] = JS_NewString(g_ctx, arg);
	argv[2] = JS_NewString(g_ctx, exception);

	JSValue ret = JS_Call(g_ctx,
		vapi.error_func,
		JS_UNDEFINED,
		countof(argv), argv
	);
	if (JS_IsException(ret)) {
		js_std_dump_error(g_ctx);
	}
	backend_response_str(500, "text/plain", "Internal server error");
}

static void
on_get(const char *url, const char *arg)
{
	/* Then call into JS */
	JSValueConst argv[2];
	argv[0] = JS_NewString(g_ctx, url);
	argv[1] = JS_NewString(g_ctx, arg);

	JSValue ret = JS_Call(g_ctx,
		vapi.backend_func,
		JS_UNDEFINED,
		countof(argv), argv
	);
	if (JS_IsException(ret)) {
		js_std_dump_error(g_ctx);
		on_error(url, "", "Unhandled JavaScript exception");
	}
	backend_response_str(404, "text/plain", "Not found");
}

static void
on_post(const char *url, const char *arg,
	const char *ctype, const uint8_t *data, size_t len)
{
	JSValueConst argv[4];
	argv[0] = JS_NewString(g_ctx, url);
	argv[1] = JS_NewString(g_ctx, arg);
	argv[2] = JS_NewString(g_ctx, ctype);
	argv[3] = JS_NewStringLen(g_ctx, (const char *)data, len);

	JSValue ret = JS_Call(g_ctx,
		vapi.post_backend_func,
		JS_UNDEFINED,
		countof(argv), argv
	);
	if (JS_IsException(ret)) {
		js_std_dump_error(g_ctx);
		on_error(url, "", "Unhandled JavaScript exception");
	}
	backend_response_str(404, "text/plain", "Not found");
}

static void storage_trampoline(
	size_t n, struct virtbuffer buffers[n], size_t reslen)
{
	JSValue sfunc =
		JS_GetPropertyStr(g_ctx, global_obj, buffers[0].data);
	assert(JS_IsFunction(g_ctx, sfunc));

	JSValueConst argv[MAX_STORAGE_ARGS];
	/* There are n - 1 arguments to the storage function. */
	for (unsigned i = 1; i < n; i++) {
		argv[i-1] = JS_NewStringLen(g_ctx, buffers[i].data, buffers[i].len);
	}

	JSValue ret = JS_Call(g_ctx,
		sfunc,
		JS_UNDEFINED,
		n - 1, argv
	);
	if (JS_IsException(ret)) {
		js_std_dump_error(g_ctx);
		/* Cleanup */
		JS_FreeValue(g_ctx, ret);
		storage_return_nothing();
		return;
	}

	size_t textlen;
	const char *text =
		JS_ToCStringLen(g_ctx, &textlen, ret);
	assert(textlen <= reslen);
	storage_return(text, textlen);
	/* Cleanup */
	JS_FreeValue(g_ctx, ret);
}

JSValue js_storage_call(JSContext *ctx,
	JSValueConst this_val, int argc, JSValueConst *argv)
{
	(void)this_val;

	if (argc >= 1) {
		if (__builtin_expect(argc > MAX_STORAGE_ARGS, 0))
			return JS_EXCEPTION;

		struct virtbuffer buffers[MAX_STORAGE_ARGS];

		for (int i = 0; i < argc; i++) {
			size_t datalen = 0;
			const char *data = "";
			data = JS_ToCStringLen(ctx, &datalen, argv[i]);
			if (data == NULL)
				return JS_EXCEPTION;
			buffers[i].data = TRUST_ME(data);
			buffers[i].len  = datalen;
		}
		const int N = argc;

		/* Make room for a decent-sized 2MB buffer */
		const size_t result_size = 1ul << 21;
		char *result = malloc(result_size);

		/* Make call into storage VM */
		long reslen = storage_callv(storage_trampoline,
			N, buffers, result, result_size);

		/* Probably an exception if reslen < 0.
		   XXX: If reslen >= result_size, not enough buffer. */
		if (reslen >= 0) {
			/* Create a JS string from the result */
			JSValue resstr = JS_NewStringLen(ctx, result, reslen);
			free(result);
			return resstr;
		}
		free(result);
	}
	return JS_EXCEPTION;
}

static void
on_live_update()
{
	JSValue sfunc =
		JS_GetPropertyStr(g_ctx, global_obj, "on_live_update");
	if (!JS_IsFunction(g_ctx, sfunc))
		return;

	JSValue ret = JS_Call(g_ctx,
		sfunc,
		JS_UNDEFINED,
		0, NULL
	);

	/* TODO: Accept ArrayBuffer as well */
	if (JS_IsString(ret))
	{
		size_t datalen;
		const char *data =
			JS_ToCStringLen(g_ctx, &datalen, ret);
		storage_return(data, datalen);
		/* Cleanup */
		JS_FreeValue(g_ctx, ret);
	} else {
		storage_return_nothing();
	}
}

static void
on_resume_update(size_t len)
{
	char* data = malloc(len);
	storage_return(data, len);

	JSValue sfunc =
		JS_GetPropertyStr(g_ctx, global_obj, "on_resume_update");
	if (!JS_IsFunction(g_ctx, sfunc)) {
		free(data);
		return;
	}

	JSValueConst argv[1];
	argv[0] = JS_NewStringLen(g_ctx, data, len);

	free(data);

	JSValue ret = JS_Call(g_ctx,
		sfunc,
		JS_UNDEFINED,
		countof(argv), argv
	);

	/* Cleanup */
	JS_FreeValue(g_ctx, ret);
}

int main(int argc, char **argv)
{
	/* QuickJS runtime */
	JSRuntime *rt = JS_NewRuntime();
	g_ctx = JS_NewContext(rt);

	js_std_init_handlers(rt);
	/* system modules */
	js_init_module_std(g_ctx, "std");
	js_init_module_os(g_ctx, "os");

	js_std_add_helpers(g_ctx, argc, argv);

	global_obj = JS_GetGlobalObject(g_ctx);

	/*** Begin VARNISH API ***/
	vapi.varnish = JS_NewObject(g_ctx);
	JS_SetPropertyStr(g_ctx, global_obj, "varnish", vapi.varnish);

	JS_SetPropertyStr(g_ctx, vapi.varnish,
		"log",
		JS_NewCFunction(g_ctx, js_log_call, "log", 1));
	JS_SetPropertyStr(g_ctx, vapi.varnish,
		"response",
		JS_NewCFunction(g_ctx, js_backend_response, "response", 3));
	JS_SetPropertyStr(g_ctx, vapi.varnish,
		"set_cacheable",
		JS_NewCFunction(g_ctx, js_cacheable_call, "set_cacheable", 4));
	JS_SetPropertyStr(g_ctx, vapi.varnish,
		"get",
		JS_NewCFunction(g_ctx, js_get_call, "get", 1));
	JS_SetPropertyStr(g_ctx, vapi.varnish,
		"set",
		JS_NewCFunction(g_ctx, js_set_call, "set", 1));
	JS_SetPropertyStr(g_ctx, vapi.varnish,
		"sendfile",
		JS_NewCFunction(g_ctx, js_sendfile_call, "sendfile", 1));
	JS_SetPropertyStr(g_ctx, vapi.varnish,
		"storage",
		JS_NewCFunction(g_ctx, js_storage_call, "storage", MAX_STORAGE_ARGS));
	JS_SetPropertyStr(g_ctx, vapi.varnish,
		"fetch",
		JS_NewCFunction(g_ctx, js_http_fetch_call, "fetch", 1));
	JS_SetPropertyStr(g_ctx, vapi.varnish,
		"post",
		JS_NewCFunction(g_ctx, js_http_post_call, "post", 3));
	/*** End Of VARNISH API ***/

	/* make 'std' and 'os' visible to non module code */
	const char *str = "import * as std from 'std';\n"
					  "import * as os from 'os';\n"
					  "globalThis.std = std;\n"
					  "globalThis.os = os;\n";
	eval_buf(g_ctx, str, strlen(str), "<input>", JS_EVAL_TYPE_MODULE);

	eval_buf(g_ctx, myjs, myjs_size, SOURCE_FILE, 0);

	vapi.backend_func =
		JS_GetPropertyStr(g_ctx, global_obj, "on_get");
	assert(JS_IsFunction(g_ctx, vapi.backend_func));
	vapi.post_backend_func =
		JS_GetPropertyStr(g_ctx, global_obj, "on_post");
	//assert(JS_IsFunction(g_ctx, vapi.post_backend_func));
	vapi.error_func =
		JS_GetPropertyStr(g_ctx, global_obj, "on_error");

	set_backend_get(on_get);
	set_backend_post(on_post);
	set_on_error(on_error);
	set_on_live_update(on_live_update);
	set_on_live_restore(on_resume_update);
	wait_for_requests();
}
