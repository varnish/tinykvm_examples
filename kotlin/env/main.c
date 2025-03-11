#define KVM_API_ALREADY_DEFINED
#include "kvm_api.h"
#include "../static_api.h"
#include <stdio.h>

static void on_get(const char *url, const char *arg)
{
	(void)arg;

	static_ExportedSymbols *syms = static_symbols();
    syms->kotlin.root.on_get(url);

    backend_response_str(404, "text/plain", "Not found");
}

int main(int argc, char** argv)
{
    static_ExportedSymbols* syms = static_symbols();
    static_kref_kotlin_Array args;
    args.pinned = NULL;
    syms->kotlin.root.main(args);
    fflush(stdout);

    set_backend_get(on_get);
    wait_for_requests();
}
