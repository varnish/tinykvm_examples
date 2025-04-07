/**
 * @file kvm_api.h
 * @author Alf-André Walla (fwsgonzo@hotmail.com)
 * @brief 
 * @version 1.0
 * @date 2025-03-11
 * 
 * Single-source-of-truth API header for the KVM and Compute VMODs.
 * Languages can use the C API directly, or indirectly through
 * API-generation based on this header.
 * 
**/
#pragma once
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
extern void register_func(int, ...);

/**
 * Detecting KVM from main(). Note that we could check CPUID, but we prefer
 * a simple check for / in argv[0].
**/
#define IS_SANDBOXED_MAIN()  (__builtin_strchr(argv[0], '/') == (void*)0)
#define IS_LINUX_MAIN()      !IS_SANDBOXED_MAIN()

/**
 * During the start of the program, one should register callback functions
 * that will handle different types of requests, like GET, POST etc.
 *
 * Example:
 *   static void on_get(const char* url, const char *arg)
 *   {
 *      http_setf(RESP, "X-Hello: World", 14);
 *      backend_response_str(200, "text/plain", "Hello World!");
 *   }
 *   int main()
 *   {
 *  	set_backend_get(on_get);
 *  	wait_for_requests();
 *   }
 *
 * The example above will register a function called 'on_get' as the
 * function that will get called on GET requests. When the main body is
 * completed, we call 'wait_for_requests()' to signal that the program has
 * successfully initialized, and is ready to handle requests. The system
 * will then pause the VM and freeze everything. The frozen state will be
 * restored on every request.
 *
 * Register callbacks for various modes of operation:
 **/
static inline void set_backend_get(void(*f)(const char *url, const char *arg)) { register_func(1, f); }
static inline void set_backend_post(void(*f)(const char *url, const char *arg, const char *ctype, const uint8_t*, size_t)) { register_func(2, f); }

struct backend_request {
	const char *method;
	const char *url;
	const char *arg;
	const char *content_type;
	uint16_t    method_len;
	uint16_t    url_len;
	uint16_t    arg_len;
	uint16_t    content_type_len;
	const uint8_t *content; /* Can be NULL. */
	size_t         content_len;
};
static inline void set_backend_request(void(*f)(const struct backend_request*)) { register_func(3, f); }

/* Streaming POST will receive each data segment as they arrive. A final POST
   call happens at the end. This call needs some further improvements, because
   right now Varnish will fill the VM with the whole POST data no matter what,
   but call the streaming POST callback on each segment. Instead, it should put
   each segment on stack and the callee may choose to build a complete buffer. */
static inline void set_backend_stream_post(long(*f)(const char *url, const char *arg, const char *ctype, const uint8_t *, size_t len, size_t off)) { register_func(4, f); }

/* When an exception happens that terminates the request it is possible to
   produce a custom response instead of a generic HTTP 500. There is very
   limited time to produce the response, typically 1.0 seconds. */
static inline void set_on_error(void (*f)(const char *url, const char *arg, const char *exception)) { register_func(5, f); }

/* When uploading a new program, there is an opportunity to pass on
   state to the next program, using the live update and restore callbacks. */
static inline void set_on_live_update(void(*f)()) { register_func(6, f); }
static inline void set_on_live_restore(void(*f)(size_t datalen)) { register_func(7, f); }

/* Wait for requests without terminating machine. Call this just before
   the end of int main(). It will preserve the state of the whole machine,
   including startup arguments and stack values. Future requests will use
   a new stack, and will not trample the old stack, including red-zone. */
extern void wait_for_requests();

/* Wait for requests by pausing the machine and recording the current
   state of machine registers. This allowed resumption in event loops
   and other types of event-driven programming. Request URL and other
   data can be retrieved from the struct backend_request argument. */
extern void wait_for_requests_paused(struct backend_request* req);

/**
 * When a request arrives the handler function is the only function that will
 * be called. main() is no longer in the picture. Any actions performed during
 * request handling will disappear after the request is processed.
 *
 * We will produce a response using the 'backend_response' function, which will
 * conclude the request.
 *
 * Example:
 *  static void on_get(const char* url, const char *arg, int req, int resp)
 *  {
 *  	const char *ctype = "text/plain";
 *  	const char *cont = "Hello World";
 *  	backend_response(200, ctype, strlen(ctype), cont, strlen(cont));
 *  }
 **/
struct ResponseHeader {
	const char *field;
	size_t      field_len;
};
struct BackendResponseExtra {
	const struct ResponseHeader *headers;
	uint16_t num_headers;
	bool cached;
	float ttl;
	float grace;
	float keep;
	uint64_t reserved[4]; /* Reserved for future use. */
};
extern void __attribute__((used))
sys_backend_response(int16_t status, const void *t, size_t, const void *c, size_t,
	const struct BackendResponseExtra *extra);

static inline void
backend_response(int16_t status, const void *t, size_t tlen, const void *c, size_t clen) {
	sys_backend_response(status, t, tlen, c, clen, NULL);
}

static inline void
backend_response_str(int16_t status, const char *ctype, const char *content)
{
	sys_backend_response(status, ctype, __builtin_strlen(ctype), content, __builtin_strlen(content), NULL);
}

/**
 * @brief Create a response that includes HTTP headers, a status code, a body and
 * a content type. The response is sent back to the client (or cached in Varnish).
 */
static inline void
backend_response_extra(int16_t status, const void *t, size_t tlen, const void *c, size_t clen,
	const struct BackendResponseExtra *extra)
{
	sys_backend_response(status, t, tlen, c, clen, extra);
}

/**
 * Stream a response back to Varnish using streaming callback function.
 * All response headers must be provided before calling this function,
 * as only data streaming is allowed afterwards. The same request VM is
 * used to deliver the response, and so all state (and stack) is available.
 * 
 * The streaming callback will ask for a limited amount of bytes, and any
 * number between 1 and max bytes can be provided. If 0 is provided, the
 * delivery is considered stalled (and fails).
 */
struct streaming_content {
	const void *data;
	size_t len;
};
typedef struct streaming_content (*content_stream_func)(void *arg, size_t max, size_t written, size_t total);
extern void __attribute__((noreturn, used))
begin_streaming_response(int16_t status, const void *t, size_t, size_t content_length, content_stream_func content_cb, const void *arg);

/**
 * HTTP header field manipulation
 *
**/
static const int REQ      = 0;
static const int RESP     = 1;
static const int REQUEST  = REQ;
static const int RESPONSE = RESP;
static const int BEREQ    = 0;
static const int BERESP   = 1;
static const unsigned HTTP_FMT_SIZE = 4096; /* Most header fields fit. */

extern long
sys_http_append(int where, const char *, size_t);

/* Append a new header field from a ZT string. */
static inline void
http_append_str(int where, const char *str) { sys_http_append(where, str, __builtin_strlen(str)); }

/* Append a new header field from a formatted string with arguments. */
static inline void
http_appendf(int where, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	char buffer[HTTP_FMT_SIZE];
	const int len = __builtin_vsnprintf(buffer, sizeof(buffer), fmt, ap);
	va_end(ap);
	sys_http_append(where, buffer, len);
}

extern long
sys_http_set(int where, const char *what, size_t len);

/* Set or overwrite an existing header field using ZT string. */
static inline void
http_set_str(int where, const char *field) { sys_http_set(where, field, __builtin_strlen(field)); }

/* Set or overwrite an existing header field using formatted string. */
static inline void
http_setf(int where, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	char buffer[HTTP_FMT_SIZE];
	const int len = __builtin_vsnprintf(buffer, sizeof(buffer), fmt, ap);
	va_end(ap);
	sys_http_set(where, buffer, len);
}

/* Unset an existing header field by key. */
static inline long
http_unset_str(int where, const char *key) { return sys_http_set(where, key, __builtin_strlen(key)); }

static inline long
http_unset(int where, const char *key, size_t len) { return sys_http_set(where, key, len); }

/* Retrieve a header field by key, writing result to outb and returning actual length.
   Returns 0 when outl is too small to hold the value.
   If outb is zero, instead returns the length of header field, or zero if not found. */
extern unsigned
sys_http_find(int where, const char *key, size_t, char *outb, size_t outl);

static inline unsigned
http_find_str(int where, const char *key, char *outb, size_t outl) {
	return sys_http_find(where, key, __builtin_strlen(key), outb, outl);
}

/* Retrieve the length of a header field by key. Returns zero when not found. */
static inline unsigned
http_find_strlen(int where, const char *key) {
	return sys_http_find(where, key, __builtin_strlen(key), NULL, 0);
}

#if __GNUC__ > 7
#define HAS_BUILTIN_MALLOC
#elif __has_builtin(__builtin_malloc)
#define HAS_BUILTIN_MALLOC
#endif

#ifdef HAS_BUILTIN_MALLOC
static inline const char *
http_alloc_find(int where, const char *key) {
	char buffer[HTTP_FMT_SIZE];
	const unsigned len = /* sys_http_find returns 0 when not found. */
		sys_http_find(where, key, __builtin_strlen(key), buffer, sizeof(buffer));
	char *result = (char *)__builtin_malloc(len+1);
	__builtin_memcpy(result, buffer, len);
	result[len] = 0;
	return result;
}
#endif

/* Retrieve the current HTTP method, eg. GET, POST etc.
   Returns 0 if the out buffer is too small. Returns the
   needed buffer length when outb is 0x0. */
extern unsigned long sys_http_method(char *outb, size_t outl);
static inline const char * http_alloc_method();

#ifdef HAS_BUILTIN_MALLOC
static inline const char * http_alloc_method() {
	char *result = (char *)__builtin_malloc(16);
	const unsigned len = sys_http_method(result, 15);
	result[len] = 0;
	return result;
}
#endif

/**
 * Regular Expressions
 *
 **/
/* Compile a new regex with the given pattern. */
extern int sys_regex_compile(const char *, size_t);
/* Free a compiled regex pattern. */
extern void sys_regex_free(int rgx);
/* Returns where the regex matches the input string, or negative. */
extern int sys_regex_match(int rgx, const char *, size_t);
/* Regex substitution on buffer with another into dst. Flags: ALL=0x1 */
extern long sys_regex_subst(int rgx, const char *buf, const char *subst, char *dst, size_t dstlen, int flags);
/* Copy header fields from one HTTP header to another. */
extern int sys_regex_copyto(int rgx, int srchp, int dsthp);

/**
 * Varnish caching configuration
 *
 **/
/* Set cacheable, ttl, grace and keep. All durations are in milliseconds. */
extern long
sys_set_cacheable(bool cached, long ttl_ms, long grace_ms, long keep_ms);

static inline long set_cacheable(bool cached, float ttl, float grace, float keep)
{
	return sys_set_cacheable(cached, ttl * 1000, grace * 1000, keep * 1000);
}

/**
 * Storage program
 *
 * Every tenant or program can have storage. If storage is enabled, then
 * it is usually the same program as the request program, as it was
 * initialized at the start. If your program has 1GB memory, then storage is
 * 1GB of memory too. Storage is just like a normal Linux program
 * in that any changes you make are never unmade. In other words, the storage
 * program has a ton of memory and is always available.
 * 
 * To access storage you can use any of the API calls below when handling a
 * request. The access is serialized, meaning only one request can access the
 * storage at a time. Because of this, one should try to keep the number and
 * the duration of the calls low, preferrably calculating as much as possible
 * before making any storage accesses.
 *
 * Storage access can be limited to certain functions by using the
 * sys_storage_allow system call, or the STORAGE_ALLOW(function) macro. If zero
 * functions are allowed, then all functions are allowed in storage. If one or
 * more functions are allowed, then only those functions can be called in
 * storage, effectively making it an allow list.
 *
 * When calling into the storage program the data you provide will be copied into
 * the storage program, and the response you give back will be copied back into
 * the request-handling program. This is extra overhead, but very safe. It is
 * very important to make transactions into the long-lived storage safe and
 * reliable.
 *
 * The purpose of storage is to enable mutable state. That is, you are
 * able to make live changes to your storage, which persists across time and
 * requests. You can also serialize the state across live updates so that it
 * can be persisted through program updates. And finally, it can be
 * saved to a file (with a very specific name) on the servers disk, so that
 * it can be persisted across normal server updates and reboots.
 *
 * This file is called 'state' and it is the only writable file a program can
 * have. It is always available to read and write. But can only be written to
 * from storage.
 **/

/* A vector-based buffer used by calls into storage. */
struct virtbuffer {
	const void *data;
	size_t      len;
};
/**
 * A callable storage function that takes a vector of buffers.
 * 'res' is the size of the return buffer where the return result can be written.
 * Inside a storage function call, the result buffer that will be returned back
 * to the request program is chosen with storage_return(). Please note that every
 * storage function must use storage_return() or storage_return_nothing().
 * 
 * Example:
 * static char ctype[] = "text/plain";
 * static char *cont = NULL;
 * static size_t cont_len = 0;
 *
 * static void
 * modify_content(size_t n, struct virtbuffer buffers[], size_t res)
 * {
 *     // Allocate a new zero-terminated buffer from input
 *     char *new_buf = malloc(buffers[0].len + 1);
 *     memcpy(new_buf, buffers[0].data, buffers[0].len);
 *     new_buf[buffers[0].len + 1] = 0;
 *
 *     free(cont);
 *
 *     // Set as new content
 *     cont = new_buf;
 *     cont_len = buffers[0].len;
 *
 *     storage_return_nothing();
 * }
 * static void
 * get_content(size_t n, struct virtbuffer buffers[], size_t res)
 * {
 *     storage_return(cont, cont_len);
 * }
 * 
 * Using the functions modify_content and get_content, we can retrieve
 * the data in storage like this:
 *
 * char buffer[65536];
 * const long len =
 *     storage_call(get_content, 0, 0, buffer, sizeof(buffer));
 *
 * Inside the storage function, the res argument will be 65536, indicating
 * how big the return buffer is.
 *
 * And we can modify the content like this:
 *
 * storage_call(modify_content, data, len, NULL, 0);
 *
 * As written before, every storage function must use storage_return() or
 * storage_return_nothing() in order to indicate the return result to the
 * request program that called the function.
 * Many programming languages require a function to run all the way to the
 * end in order to free temporary objects and allocations. Because of this,
 * the storage function does not end after the call to storage_return(),
 * instead the function is resumed afterwards in order to fully return
 * from the function. This allows the system to copy the data back to the
 * request program before the function has returned and deallocated the data.
**/
typedef void (*storage_func) (size_t n, struct virtbuffer[], size_t res);

/* Returns true (1) if called from storage. */
extern int
sys_is_storage();

/* Transfer an array of buffers to storage, transfer output into @dst. */
extern long
storage_callv(storage_func, size_t n, const struct virtbuffer[], void* dst, size_t);

/* Transfer an array to storage, transfer output into @dst. */
static inline long
storage_call(storage_func func, const void *src, size_t len, void *res, size_t reslen) {
	const struct virtbuffer buf[1] = {
		{ .data = src, .len = len }
	};
	return storage_callv(func, 1, buf, res, reslen);
}

/* Create a task in storage that is scheduled to run next.
   If start or period is set, the task will be scheduled to run after
   start milliseconds, and then run every period milliseconds. The
   system call returns the timer id.
   If it is a periodic task, it will return a task id. */
typedef void (*storage_task_func) (void *data, size_t len);

extern long
sys_storage_task(storage_task_func, const void* data, size_t len, uint64_t start, uint64_t period);

/* Schedule a storage task to happen next. It will be queued up for storage access. */
static inline long
storage_task(storage_task_func task, const void *data, size_t len) { return sys_storage_task(task, data, len, 0, 0); }

/* Schedule a storage task to happen at some point. It will be queued up for storage access periodically. */
static inline long
schedule_storage_task(storage_task_func task, const void *data, size_t len, float start, float period) {
	return sys_storage_task(task, data, len, start * 1000, period * 1000);
}

/* Stop a previously scheduled task. Returns TRUE on success. */
extern long
stop_storage_task(long task);

/* Used to return data from a storage function, and then return and complete the function.
   NOTE: This function *always* returns back allowing cleanup, such as destructors. */
extern void
storage_return(const void* data, size_t len);

static inline void
storage_return_nothing(void) { storage_return(NULL, 0); }

/* Used to return data from storage functions, and never returns back to finish the function.
   NOTE: The function *never* returns back, preventing cleanup and destructors from running. */
extern void __attribute__ ((noreturn))
storage_noreturn(const void* data, size_t len);

/* Allow a certain function to be called from a request VM.
   If this function is never called, all functions are allowed. */
extern long sys_storage_allow(void(*)());
#define STORAGE_ALLOW(x) sys_storage_allow((void(*)())x)

/* Start multi-processing using @n vCPUs on given function,
   forwarding up to 4 integral/pointer arguments.
   Multi-processing starts and ends asynchronously.
   The n argument is the number of total CPUs that will exist
   in the system, and n is required to be at least 2 to start
   one additional CPU. Use vcpuid() to retrieve the current
   vCPU id during asynchronous operation.

   Example usage:
	// Start 7 additional vCPUs, each running the given function with the
	// same provided argument:
	multiprocess(8, (multiprocess_t)dotprod_mp_avx, &data);
	// Run the first portion on the current vCPU (with id 0):
	dotprod_mp_avx(&data);
	// Wait for the asynchronous multi-processing operation to complete:
	multiprocess_wait();
*/
typedef void(*multiprocess_t)(void*);
extern long multiprocess(size_t n, multiprocess_t func, void*);

/* Start multi-processing using @n vCPUs on given function,
   forwarding an array with the given array element size.
   The array must outlive the vCPU asynchronous operation.

   Example usage:
	// Start 7 additional vCPUs
	multiprocess_array(8, dotprod_mp_avx, &data, sizeof(data[0]));
	// Run the first portion on the main vCPU (with id 0)
	dotprod_mp_avx(&data[0], sizeof(data[0]));
	// Wait for the asynchronous operation to complete
	multiprocess_wait();
*/
typedef void(*multiprocess_array_t)(int, void* array, size_t element_size);
extern long multiprocess_array(size_t n,
	multiprocess_array_t func, void* array, size_t element_size);

/* Start multi-processing using @n vCPUs at the current RIP,
   forwarding all registers except RFLAGS, RSP, RBP, RAX, R14, R15.
   Those registers are clobbered and have undefined values.

   Stack size is the size of one individual stack, and the caller
   must make room for n stacks of the given size. Example:
   void* stack_base = malloc(n * stack_size);
   multiprocess_clone(n, stack_base, stack_size);
*/
extern long multiprocess_clone(size_t n, void* stack_base, size_t stack_size);

/* Wait until all multi-processing workloads have ended running. */
extern long multiprocess_wait();

/* Returns the current vCPU ID. Used in processing functions during
   multi-processing operation. */
extern int vcpuid() __attribute__((const));


/* Shared memory between all VMs. Globally visible to
   all VMs, and reads/writes are immediately seen
   by both storage and request VMs. */
struct shared_memory_info {
	uint64_t ptr;
	uint64_t end;
};
extern struct shared_memory_info shared_memory_area();

/* Allocate pointers to shared memory with given size and alignment. */
#define SHM_ALLOC_BYTES(x) internal_shm_alloc(x, 8)
#ifdef __cplusplus
#define SHM_ALLOC_TYPE(x) (x *)internal_shm_alloc(sizeof(x), alignof(x))
#define SHM_ALLOC(x) (decltype(x) *)internal_shm_alloc(sizeof(x), alignof(x))
#else
#define SHM_ALLOC_TYPE(x) (x *)internal_shm_alloc(sizeof(x), _Alignof(x))
#define SHM_ALLOC(x) (typeof(x) *)internal_shm_alloc(sizeof(x), _Alignof(x))
#endif
static inline void * internal_shm_alloc(size_t size, size_t align) {
	static struct shared_memory_info info;
	if (info.ptr == 0x0) {
		info = shared_memory_area();
	}
	char *p = (char *)((info.ptr + (align-1)) & ~(uint64_t)(align-1));
	info.ptr = (uint64_t)&p[size];
	if ((uint64_t)p + size <= info.end)
		return p;
	else
		return NULL;
}

/* Setting this during initialization will determine whether
   or not request VMs will be reset after each request.
   When they are ephemeral, they will be reset.
   This setting is ENABLED by default for security reasons and
   cannot be disabled, except by allowing it through a tenant
   configuration setting. */
extern int sys_make_ephemeral(int);

/* Retrieve information about memory limits. */
struct meminfo {
	uint64_t max_memory;
	uint64_t max_reqmem;
	uint64_t reqmem_upper;
	uint64_t reqmem_current;
};
extern void get_meminfo(struct meminfo*);

/* Logging to Varnish Log */
extern void sys_log(const char *, size_t);
static inline void vlogf(const char *fmt, ...)
{
	char buffer[2048];
	va_list va;
	va_start(va, fmt);
	/* NOTE: vsnprintf has an insane return value. */
	const int len = __builtin_vsnprintf(buffer, sizeof(buffer), fmt, va);
	va_end(va);
	if (len >= 0 && (size_t)len < sizeof(buffer)) {
		sys_log(buffer, len);
	} else {
		static const char error_string[] = "(ERROR: Log buffer overflowed)";
		sys_log(error_string, sizeof(error_string)-1);
	}
}

/* Returns true (1) if this program is uploaded through the live-debug
   VCL function, and is a debug program. Otherwise, false (0). */
extern int sys_is_debug();

/* Live-debugging breakpoint. Live-debugging can only be used
   by debug programs. This breakpoint is a no-op for regular
   programs. Debug programs cannot be debugged without a
   breakpoint, as the brekapoint is the single-source of opening
   up for remote GDB debugging. */
extern void sys_breakpoint();

/**
 * cURL and HTTP request-related functions
**/

/* Fetch content from provided URL. Content will be allocated by Varnish
   when fetching. The fetcher will also fill out the input structure if the
   fetch succeeds. If a serious error is encountered, the function returns
   a non-zero value and the struct contents are undefined.
   If any of the headers or headers_length fields are non-zero, cURL will
   ***NOT*** read response headers into them and set the values. It is a
   way to disable the feature, as it takes up extra memory if unwanted.
   *curl_fields* and *curl_options* are both optional and can be NULL.
   NOTE: You can avoid running out of memory by pre-allocating storage
   for the requests if using non-ephemeral VMs. */
struct curl_op {
	uint32_t    status;
	uint32_t    post_buflen;
	const void *post_buffer;
	char    *headers;
	uint32_t headers_length;
	uint32_t unused1;
	void    *content;
	uint32_t content_length;
	uint32_t ctlen;
	char   ctype[128];
};
struct curl_fields {
#define CURL_FIELDS_COUNT  12u
	const char *ptr[CURL_FIELDS_COUNT];
	uint16_t    len[CURL_FIELDS_COUNT];
};
#define CURL_SETFIELD(f, idx, value) { f.ptr[idx] = value; f.len[idx] = __builtin_strlen(value); }
struct curl_options {
	const char *interface;       /* Select interface to bind to. */
	const char *unused;
	int8_t      follow_location; /* Follow Location in 301. */
	int8_t      dummy_fetch;     /* Does not allocate content. */
	int8_t      tcp_fast_open;   /* Enables TCP Fast Open. */
	int8_t      dont_verify_host;
	uint32_t    unused_opt5;
};
extern long sys_fetch(const char*, size_t, struct curl_op*, struct curl_fields*, struct curl_options*);

/* Varnish self-request */
extern long sys_request(const char*, size_t, struct curl_op*, struct curl_fields*, struct curl_options*);

/**
 * Utility functions
**/

/* Embed binary data into executable. This data has no guaranteed alignment. */
#define EMBED_BINARY(name, filename) \
	asm(".pushsection .rodata\n" \
	"	.global " #name "\n" \
	#name ":\n" \
	"	.incbin " #filename "\n" \
	#name "_end:\n" \
	"	.int  0\n" \
	"	.global " #name "_size\n" \
	"	.type   " #name "_size, @object\n" \
	"	.align 4\n" \
	#name "_size:\n" \
	"	.int  " #name "_end - " #name "\n" \
	".popsection"); \
	extern char name[]; \
	extern unsigned name ##_size;

#define TRUST_ME(ptr)    ((void*)(uintptr_t)(ptr))

#ifndef KVM_API_ALREADY_DEFINED
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

asm(".global wait_for_requests_paused\n"
	".type wait_for_requests_paused, @function\n"
	"wait_for_requests_paused:\n"
	"	mov $0x10002, %eax\n"
	"	out %eax, $0\n"
	"	ret\n");

asm(".global sys_set_cacheable\n"
	".type sys_set_cacheable, @function\n"
	"sys_set_cacheable:\n"
	"	mov $0x10005, %eax\n"
	"	out %eax, $0\n"
	"	ret\n");

asm(".global sys_backend_response\n"
	".type sys_backend_response, @function\n"
	"sys_backend_response:\n"
	".cfi_startproc\n"
	"	mov $0x10010, %eax\n"
	"	out %eax, $0\n"
	"	ret\n"
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

asm(".global vcpureqid\n"
	".type vcpureqid, @function\n"
	"vcpureqid:\n"
	"	mov %gs:(0x4), %eax\n"
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

#endif // KVM_API_ALREADY_DEFINED

#ifdef __cplusplus
}
#endif
