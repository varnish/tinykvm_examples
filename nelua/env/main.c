#include "kvm_api.h"

void backend_response1(int16_t status, const char *t, size_t tlen, const char *c, size_t clen) {
	sys_backend_response(status, t, tlen, c, clen, NULL);
}
