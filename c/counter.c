#include "../kvm_api.h"
#include <string.h>
#include <stdio.h>
static int *counter;

static void
on_get(const char *url, const char *arg)
{
	__sync_fetch_and_add(counter, 1);

	char buffer[64];
	const int len =
		snprintf(buffer, sizeof(buffer), "Hello %d World!", *counter);

	backend_response(200, "text/plain", strlen("text/plain"), buffer, len);
}

int main(int argc, char **argv)
{
	/* Allocate an integer from shared memory area */
	counter = SHM_ALLOC(*counter);

	set_backend_get(on_get);
	wait_for_requests();
}
