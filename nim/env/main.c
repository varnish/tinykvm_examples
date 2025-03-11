#define KVM_API_ALREADY_DEFINED
#include "kvm_api.h"
#include <assert.h>
typedef void (*callback_func_t)();
extern void backend_get_trampoline(callback_func_t, const char *url, const char *arg);
extern void backend_post_trampoline(callback_func_t, const char *url, const char *arg, const char *, const uint8_t *, size_t);
extern void backend_error_trampoline(callback_func_t, const char *url, const char *arg, const char *err);

static callback_func_t request_handler = NULL;
extern void set_get_handler(callback_func_t handler) {
	request_handler = handler;
}

static void
nim_get(const char *url, const char *arg)
{
	/* Nim backend get */
	if (request_handler) {
		backend_get_trampoline(request_handler, url, arg);
	}
	/* Fallback response in case Nim doesn't generate one */
	static const char cttype[] = "text/plain";
	static const char content[] = "No response";
	backend_response(404, cttype, sizeof(cttype)-1, content, sizeof(content)-1);
}

static callback_func_t post_handler = NULL;
extern void set_post_handler(callback_func_t handler) {
	post_handler = handler;
}

static void
nim_post(const char *url, const char *arg,
	const char *ctype, const uint8_t *data, size_t len)
{
	/* Nim backend post */
	if (post_handler) {
		backend_post_trampoline(post_handler, url, arg, ctype, data, len);
	}
	/* Fallback response in case Nim doesn't generate one */
	static const char cttype[] = "text/plain";
	static const char content[] = "No response";
	backend_response(404, cttype, sizeof(cttype)-1, content, sizeof(content)-1);
}

static callback_func_t error_handler = NULL;
extern void set_error_handler(callback_func_t handler) {
	error_handler = handler;
}

static void
nim_error(const char *url, const char *arg, const char *error)
{
	/* Nim backend error */
	if (error_handler) {
		backend_error_trampoline(error_handler, url, arg, error);
	}
	/* Fallback response in case Nim doesn't generate one */
	backend_response_str(500, "text/plain", error);
}

extern void NimMain();
int main()
{
	NimMain();
	set_backend_get(nim_get);
	set_backend_post(nim_post);
	set_on_error(nim_error);
	wait_for_requests();
}

typedef void (*nim_trampoline_cb)(void (*)(), const char *, size_t);
typedef void (*nim_storage_cb) ();

static void storage_trampoline(size_t n, struct virtbuffer buffers[n], size_t res)
{
	assert(n == 3 && res > 0);
	nim_trampoline_cb trampoline = *(nim_trampoline_cb *)buffers[0].data;
	nim_storage_cb user_callback = *(nim_storage_cb *)buffers[1].data;

	trampoline(user_callback, buffers[2].data, buffers[2].len);
	/* Fallback: Return nothing. */
	storage_return_nothing();
}
long nim_storage(void (*callback)(), void (*trampoline)(),
	const char *input, size_t inplen, char *res, size_t reslen)
{
	const struct virtbuffer buffers[3] = {
		{&trampoline, sizeof(trampoline)},
		{&callback, sizeof(callback)},
		{TRUST_ME(input), inplen}
	};
	return storage_callv(storage_trampoline, 3, buffers, res, reslen);
}
long nim_storage_task(storage_task_func callback, const void *arg, size_t len, float start, float period)
{
	return schedule_storage_task(callback, arg, len, start, period);
}
void nim_finish_storage(const char *result, size_t len)
{
	storage_return(result, len);
}

// Memory information
long nim_workmem_max()
{
	struct meminfo info;
	get_meminfo(&info);
	return info.max_reqmem;
}
long nim_workmem_current()
{
	struct meminfo info;
	get_meminfo(&info);
	return info.reqmem_current;
}
