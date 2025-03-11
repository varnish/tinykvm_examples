#include "kvm_api.hpp"
#include <cstring>
#define REQ  0u
#define RESP 2u

static void
on_recv(const char *url)
{
	//Http::append(REQ, "X-Hello: World");
}

int main(int argc, char **argv)
{
	register_func(0, on_recv);
	wait_for_requests();
}
