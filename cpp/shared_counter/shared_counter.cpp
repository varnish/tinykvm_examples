#include "../../kvm_api.h"
static uint64_t* shared_counter = nullptr;

static void on_get(const char *url, const char*)
{
	uint64_t counter = __sync_fetch_and_add(shared_counter, 1);

	const char ct[] = "text/plain";
	char co[] = "Hell0 World, 000!";
	co[ 4] = '0' + (counter / 1000) % 10;
	co[13] = '0' + (counter / 100) % 10;
	co[14] = '0' + (counter / 10) % 10;
	co[15] = '0' + (counter / 1) % 10;
	backend_response(200, ct, sizeof(ct)-1, co, sizeof(co)-1);
}

int main()
{
	// Allocate a shared counter in shared memory
	shared_counter = SHM_ALLOC_TYPE(uint64_t);

	set_backend_get(on_get);
	wait_for_requests();
}
