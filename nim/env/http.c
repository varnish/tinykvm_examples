#define KVM_API_ALREADY_DEFINED
#include "kvm_api.h"
#include <stdio.h>
#include <string.h>

static struct curl_op cop = {};
struct curl_options options = {};

void curl_fetch_wrapper(const char *url, size_t len)
{
	cop.post_buffer = NULL;
	cop.post_buflen = 0;

	options = (struct curl_options){};
	options.tcp_fast_open = 1;

	if (sys_fetch(url, len, &cop, NULL, &options) < 0)
	{
		cop.status = 0;
		cop.content = "";
		cop.content_length = 0;
		cop.ctlen = 0;
	}
}

void curl_post_wrapper(const char *url, size_t len, const char *postct, size_t postctlen, const char *post, size_t postlen)
{
	cop.post_buffer = post;
	cop.post_buflen = postlen;

	options = (struct curl_options){};
	options.tcp_fast_open = 1;

	/* POST requests use the content-type for both in/out */
	cop.ctlen = postctlen;
	if (postctlen > 0)
		memcpy(cop.ctype, postct, postctlen);
	if (sys_fetch(url, len, &cop, NULL, &options) < 0)
	{
		cop.status = 0;
		cop.content = "";
		cop.content_length = 0;
		cop.ctlen = 0;
	}
}

long curl_get_status()
{
	return cop.status;
}
long curl_get_content_length()
{
	return cop.content_length;
}
const char *curl_get_content()
{
	return cop.content;
}
long curl_get_content_type_length()
{
	return cop.ctlen;
}
const char *curl_get_content_type()
{
	return &cop.ctype[0];
}
