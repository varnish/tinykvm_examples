#include "../kvm_api.h"
#include <string.h>
#include <stdio.h>

int main(int argc, char **argv)
{
	/* If arg0 contains / we assume it's run from terminal. */ 
	if (IS_LINUX_MAIN()) {
		puts("Hello Linux World!");
		return 0;
	}

	while (true) {
		struct backend_request req;
		wait_for_requests_paused(&req);

		http_appendf(RESP, "X-Hello: %s", "World");
		backend_response_str(200, "text/plain", "Hello World!");
	}
}
