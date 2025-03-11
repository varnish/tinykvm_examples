#include "../../kvm_api.h"
static uint64_t counter = 0;

void storage_add_counter(uint64_t increment)
{
	__sync_fetch_and_add(&counter, increment);
}

uint64_t storage_counter()
{
	return counter;
}

static void on_live_update()
{
	storage_return(&counter, sizeof(counter));
}

static void on_resume_update(size_t len)
{
	storage_return(&counter, len);
}

extern "C" void _start()
{
	STORAGE_ALLOW(storage_add_counter);
	STORAGE_ALLOW(storage_counter);

	set_on_live_update(on_live_update);
	set_on_live_restore(on_resume_update);

    wait_for_requests();
}
