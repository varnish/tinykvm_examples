#define KVM_API_ALREADY_DEFINED 1
#include "kvm_api.h"
#include <stdlib.h>

extern void go_get_handler(char *url, char *arg);

extern void go_post_handler(char *url, char *arg, char *ctype, void *content, size_t len);

extern void go_backend_response(int16_t status,
    const char *ctype, size_t ctlen, const uint8_t *data, size_t datalen);

extern long go_set_cacheable(bool cached, float ttl, float grace, float keep);

extern void __attribute__((noreturn)) setup_varnish_for_go();

static inline struct virtbuffer virtbuffer_at(struct virtbuffer *buf, size_t n) {
	return buf[n];
}

extern void storage_trampoline(size_t n, struct virtbuffer[n], size_t res);
extern void storage_strings_trampoline(size_t n, struct virtbuffer[n], size_t res);

static inline void
go_storage_init()
{
	STORAGE_ALLOW(storage_trampoline);
	STORAGE_ALLOW(storage_strings_trampoline);
}

extern long go_storage_call(void *func, size_t funclen, const void *src, size_t len, void *result, size_t resmax);
extern long go_execute_storage_call_strings(struct virtbuffer* bufs, size_t n, void *result, size_t resmax);
