#include "kvm_api.hpp"

static void on_get(const char *url, const char *arg)
{
	Backend::response(200, url, arg);
}

int main()
{
	set_backend_get(on_get);
	wait_for_requests();
}
