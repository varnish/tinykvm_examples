#pragma once
#include <stddef.h>

extern void curl_fetch_wrapper(const char *url, size_t len);
extern void curl_post_wrapper(const char *url, size_t len, const char *postct, size_t postctlen, const char *post, size_t postlen);

extern int curl_get_status();
extern const char *curl_get_content();
extern long curl_get_content_length();
extern const char *curl_get_content_type();
extern long curl_get_content_type_length();
