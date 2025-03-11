#include "goapi.h"
#include <assert.h>
#include <stdlib.h>

extern void go_backend_response(int16_t status,
    const char *ctype, size_t ctlen, const uint8_t *data, size_t datalen)
{
    backend_response(status, ctype, ctlen, data, datalen);
}

extern long go_set_cacheable(bool cached, float ttl, float grace, float keep)
{
    return sys_set_cacheable(cached, ttl * 1000, grace * 1000, keep * 1000);
}

extern void
setup_varnish_for_go()
{
	go_storage_init();

    set_backend_get((void(*)(const char *, const char *))go_get_handler);
	set_backend_post((void(*)(const char *, const char *, const char *, const uint8_t*, size_t))go_post_handler);
    wait_for_requests();
	__builtin_unreachable();
}

/* Single and array-based storage calls */
extern void storage_trampoline(size_t n, struct virtbuffer[n], size_t res);
extern void storage_strings_trampoline(size_t n, struct virtbuffer[n], size_t res);

extern long
go_storage_call(void *func, size_t funclen, const void *src, size_t len, void *result, size_t resmax) {
	const struct virtbuffer buf[2] = {
		{ .data = func, .len = funclen },
		{ .data = src, .len = len }
	};

	return storage_callv(storage_trampoline, 2, buf, result, resmax);
}

extern long
go_execute_storage_call_strings(struct virtbuffer *bufs, size_t n, void *result, size_t resmax)
{
	return storage_callv(storage_strings_trampoline, n, bufs, result, resmax);
}


asm(".global register_func\n"
	".type register_func, @function\n"
	"register_func:\n"
	".cfi_startproc\n"
	"	mov $0x10000, %eax\n"
	"	out %eax, $0\n"
	"	ret\n"
	".cfi_endproc\n");

asm(".global wait_for_requests\n"
	".type wait_for_requests, @function\n"
	"wait_for_requests:\n"
	"	mov $0x10001, %eax\n"
	"	out %eax, $0\n");

asm(".global sys_set_cacheable\n"
	".type sys_set_cacheable, @function\n"
	"sys_set_cacheable:\n"
	"	mov $0x10005, %eax\n"
	"	out %eax, $0\n"
	"	ret\n");

asm(".global backend_response\n"
	".type backend_response, @function\n"
	"backend_response:\n"
	".cfi_startproc\n"
	"	mov $0x10010, %eax\n"
	"	out %eax, $0\n"
	".cfi_endproc\n");

asm(".global begin_streaming_response\n"
	".type begin_streaming_response, @function\n"
	"begin_streaming_response:\n"
	".cfi_startproc\n"
	"	mov $0x10012, %eax\n"
	"	out %eax, $0\n"
	".cfi_endproc\n");

asm(".global sys_http_append\n"
	".type sys_http_append, @function\n"
	"sys_http_append:\n"
	"	mov $0x10020, %eax\n"
	"	out %eax, $0\n"
	"	ret\n");

asm(".global sys_http_set\n"
	".type sys_http_set, @function\n"
	"sys_http_set:\n"
	"	mov $0x10021, %eax\n"
	"	out %eax, $0\n"
	"	ret\n");

asm(".global sys_http_find\n"
	".type sys_http_find, @function\n"
	"sys_http_find:\n"
	"	mov $0x10022, %eax\n"
	"	out %eax, $0\n"
	"	ret\n");

asm(".global sys_http_method\n"
	".type sys_http_method, @function\n"
	"sys_http_method:\n"
	"	mov $0x10023, %eax\n"
	"	out %eax, $0\n"
	"	ret\n");

asm(".global sys_regex_compile\n"
	".type sys_regex_compile, @function\n"
	"sys_regex_compile:\n"
	"	mov $0x10030, %eax\n"
	"	out %eax, $0\n"
	"	ret\n");

asm(".global sys_regex_free\n"
	".type sys_regex_free, @function\n"
	"sys_regex_free:\n"
	"	mov $0x10031, %eax\n"
	"	out %eax, $0\n"
	"	ret\n");

asm(".global sys_regex_match\n"
	".type sys_regex_match, @function\n"
	"sys_regex_match:\n"
	"	mov $0x10032, %eax\n"
	"	out %eax, $0\n"
	"	ret\n");

asm(".global sys_regex_subst\n"
	".type sys_regex_subst, @function\n"
	"sys_regex_subst:\n"
	"	mov $0x10033, %eax\n"
	"	out %eax, $0\n"
	"	ret\n");

asm(".global sys_regex_copyto\n"
	".type sys_regex_copyto, @function\n"
	"sys_regex_copyto:\n"
	"	mov $0x10035, %eax\n"
	"	out %eax, $0\n"
	"	ret\n");

asm(".global sys_adns_new\n"
	".type sys_adns_new, @function\n"
	"sys_adns_new:\n"
	"	mov $0x10200, %eax\n"
	"	out %eax, $0\n"
	"	ret\n");

asm(".global sys_adns_free\n"
	".type sys_adns_free, @function\n"
	"sys_adns_free:\n"
	"	mov $0x10201, %eax\n"
	"	out %eax, $0\n"
	"	ret\n");

asm(".global sys_adns_config\n"
	".type sys_adns_config, @function\n"
	"sys_adns_config:\n"
	"	mov $0x10202, %eax\n"
	"	out %eax, $0\n"
	"	ret\n");

asm(".global sys_adns_get\n"
	".type sys_adns_get, @function\n"
	"sys_adns_get:\n"
	"	mov $0x10203, %eax\n"
	"	out %eax, $0\n"
	"	ret\n");

asm(".global shared_memory_area\n"
	".type shared_memory_area, @function\n"
	"shared_memory_area:\n"
	"	mov $0x10700, %eax\n"
	"	out %eax, $0\n"
	"   ret\n");

asm(".global sys_make_ephemeral\n"
	".type sys_make_ephemeral, @function\n"
	"sys_make_ephemeral:\n"
	"	mov $0x10703, %eax\n"
	"	out %eax, $0\n"
	"   ret\n");

asm(".global sys_is_storage\n"
	".type sys_is_storage, @function\n"
	"sys_is_storage:\n"
	"	mov $0x10706, %eax\n"
	"	out %eax, $0\n"
	"   ret\n");

asm(".global sys_storage_allow\n"
	".type sys_storage_allow, @function\n"
	"sys_storage_allow:\n"
	"	mov $0x10707, %eax\n"
	"	out %eax, $0\n"
	"   ret\n");

asm(".global storage_callv\n"
	".type storage_callv, @function\n"
	"storage_callv:\n"
	"	mov $0x10708, %eax\n"
	"	out %eax, $0\n"
	"   ret\n");

asm(".global sys_storage_task\n"
	".type sys_storage_task, @function\n"
	"sys_storage_task:\n"
	"	mov $0x10709, %eax\n"
	"	out %eax, $0\n"
	"   ret\n");

asm(".global stop_storage_task\n"
	".type stop_storage_task, @function\n"
	"stop_storage_task:\n"
	"	mov $0x1070A, %eax\n"
	"	out %eax, $0\n"
	"   ret\n");

asm(".global storage_return\n"
	".type storage_return, @function\n"
	"storage_return:\n"
	".cfi_startproc\n"
	"	mov $0x10011, %eax\n"
	"	out %eax, $0\n"
	"	ret\n"
	".cfi_endproc\n");

asm(".global storage_noreturn\n"
	".type storage_noreturn, @function\n"
	"storage_noreturn:\n"
	".cfi_startproc\n"
	"	mov $0x10013, %eax\n"
	"	out %eax, $0\n"
	".cfi_endproc\n");

asm(".global multiprocess\n"
	".type multiprocess, @function\n"
	"multiprocess:\n"
	"	mov $0x10710, %eax\n"
	"	out %eax, $0\n"
	"   ret\n");

asm(".global multiprocess_array\n"
	".type multiprocess_array, @function\n"
	"multiprocess_array:\n"
	"	mov $0x10711, %eax\n"
	"	out %eax, $0\n"
	"   ret\n");

asm(".global multiprocess_clone\n"
	".type multiprocess_clone, @function\n"
	"multiprocess_clone:\n"
	"	mov $0x10712, %eax\n"
	"	out %eax, $0\n"
	"   ret\n");

asm(".global multiprocess_wait\n"
	".type multiprocess_wait, @function\n"
	"multiprocess_wait:\n"
	"	mov $0x10713, %eax\n"
	"	out %eax, $0\n"
	"   ret\n");

asm(".global vcpuid\n"
	".type vcpuid, @function\n"
	"vcpuid:\n"
	"	mov %gs:(0x0), %eax\n"
	"   ret\n");

asm(".global get_meminfo\n"
	".type get_meminfo, @function\n"
	"get_meminfo:\n"
	"	mov $0x10A00, %eax\n"
	"	out %eax, $0\n"
	"   ret\n");

asm(".global sys_fetch\n"
	".type sys_fetch, @function\n"
	"sys_fetch:\n"
	"	mov $0x20000, %eax\n"
	"	out %eax, $0\n"
	"   ret\n");

asm(".global sys_request\n"
	".type sys_request, @function\n"
	"sys_request:\n"
	"	mov $0x20001, %eax\n"
	"	out %eax, $0\n"
	"   ret\n");

asm(".global sys_log\n"
	".type sys_log, @function\n"
	"sys_log:\n"
	"	mov $0x7F000, %eax\n"
	"	out %eax, $0\n"
	"   ret\n");

asm(".global sys_is_debug\n"
	".type sys_is_debug, @function\n"
	"sys_is_debug:\n"
	"	mov $0x7FDEB, %eax\n"
	"	out %eax, $0\n"
	"   ret\n");

asm(".global sys_breakpoint\n"
	".type sys_breakpoint, @function\n"
	"sys_breakpoint:\n"
	"	mov $0x7F7F7, %eax\n"
	"	out %eax, $0\n"
	"   ret\n");
