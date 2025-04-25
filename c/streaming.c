#include "../kvm_api.h"
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

static const char data[] = "Hello Varnish World!";
static const size_t datalen = sizeof(data)-1;

static struct streaming_content
stream(void *arg, size_t max, size_t written, size_t total)
{
    const char *d = (const char *)arg;
    /* Write one byte at a time for maximum efficiency and performance. */
    return (struct streaming_content){&d[written], 1};
}

static void
on_get(const char *url, const char *arg)
{
    (void)url;
    set_cacheable(false, 0.01, 0.0, 0.0);

    static const char *ctype = "text/plain";
    begin_streaming_response(200, ctype, strlen(ctype), datalen, stream, data);
}

int main(int argc, char **argv)
{
    if (IS_LINUX_MAIN())
    {
        puts("Hello Linux World!");
        return 0;
    }

    set_backend_get(on_get);
    wait_for_requests();
}
