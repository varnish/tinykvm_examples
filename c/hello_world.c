#include "../kvm_api.h"
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

static void
on_get(const char *url, const char *arg)
{
	http_appendf(RESP, "X-Hello: %s", "World");

	const char *hello = http_alloc_find(RESP, "X-Hello");
	assert(strcmp(hello, "X-Hello: World") == 0);

	set_cacheable(false, 0.01, 0.0, 0.0);
	backend_response_str(200, "text/plain", "Hello World!");
}

int main(int argc, char **argv)
{
	/* If arg0 contains / we assume it's run from terminal. */ 
	if (strchr(argv[0], '/')) {
		puts("Hello Linux World!");
		return 0;
	}

	set_backend_get(on_get);
	wait_for_requests();
}
