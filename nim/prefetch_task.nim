import varnish, strutils
import random

var example_content: string

proc prefetcher(arg: cstring, len: int) {.cdecl} =
    echo "prefetcher: ", $arg
    let res = Curl.fetch($arg)
    example_content = res.content

proc get_content(_: string): string {.cdecl} =
    return example_content

proc on_get(url: string, arg: string): (int, string, string) {.cdecl} =
    if url == "/nim_prefetch_example":
        set_cacheable(false, 1.0f, 0.0f, 0.0f)
        return (200, "text/plain", "Example Content" & $rand(6))

    #var res = Curl.post("http://httpbin.org/post", "text/plain", "data")
    Http.set("X-Memory-Usage: " & formatSize(nim_workmem_current()))

    set_cacheable(false, 1.0f, 0.0f, 0.0f)
    (200, "text/plain", storage(get_content, ""))


set_get_handler(on_get)

randomize()
# Put /example in cache immediately after uploading the program.
storage_task(prefetcher, "/nim_prefetch_example")
