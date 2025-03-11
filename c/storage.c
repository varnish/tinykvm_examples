#include "../kvm_api.h"
#include <malloc.h>
#include <string.h>

static char ctype[] = "text/plain";
static char *cont = NULL;
static size_t cont_len = 0;

/* Storage function that creates new 'cont' from input buffer. */
static void
modify_content(size_t n, struct virtbuffer buffers[n], size_t res)
{
	/* Allocate a new zero-terminated buffer from input */
	char *new_buf = malloc(buffers[0].len + 1);
	memcpy(new_buf, buffers[0].data, buffers[0].len);
	new_buf[buffers[0].len + 1] = 0;

	free(cont);

	/* Set as new content */
	cont = new_buf;
	cont_len = buffers[0].len;

	storage_return_nothing();
}
static void
get_content(size_t n, struct virtbuffer buffers[n], size_t res)
{
	storage_return(cont, cont_len);
}

/* GET requests go here. */
static void
on_get(const char *url, const char *arg)
{
	http_append_str(RESP, "X-Hello: World");

	/* Get current content from storage. */
	char buffer[65536];
	const long len =
		storage_call(get_content, 0, 0, buffer, sizeof(buffer));

	/* Uncacheable, 10s TTL, 0s grace, 0s keep. */
	set_cacheable(0, 10.0f, 0.0, 0.0);

	backend_response(200, ctype, strlen(ctype), buffer, len);
}

/* POST requests go here. */
static void
on_post(const char *url, const char *arg,
	const char *content_type, const uint8_t *data, size_t len)
{
	(void)content_type;

	storage_call(modify_content, data, len, NULL, 0);

	backend_response_str(200, "text/plain", "OK");
}

/* Remembering 'cont' across live program updates. */
static void
on_live_update()
{
	storage_return(cont, strlen(cont));
}
static void
on_resume_update(size_t len)
{
	free(cont);
	cont = malloc(len);
	storage_return(cont, len);
}

static void test(void *arg)
{
	/* The return address is on the usercode page (0x5000). */
	void *ra = __builtin_return_address(0);

	char buffer[1024];
	snprintf(buffer, sizeof(buffer), "/%p", ra);

	struct curl_op op = {};
	sys_fetch(buffer, strlen(buffer), &op, NULL, NULL);
}

/* Startup is executing in storage. */
int main(int argc, char **argv)
{
	/* Create the first content. */
	const char hw[] = "Hello World!";
	cont = malloc(sizeof(hw));
	memcpy(cont, hw, sizeof(hw));

	/* Fetch from ourselves every 1.5 seconds. */
	schedule_storage_task(test, NULL, 0, 0.0, 1.5);

	/* Callbacks needed to operate this program. */
	set_backend_get(on_get);
	set_backend_post(on_post);
	set_on_live_update(on_live_update);
	set_on_live_restore(on_resume_update);
	wait_for_requests();
}
