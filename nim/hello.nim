import varnish, strutils

proc on_get(url: string, arg: string): (int, string, string) {.cdecl} =
    var res = Curl.post("http://127.0.0.1:7070/post", "text/plain", "data")

    breakpoint()

    Http.set("X-Memory-Usage: " & formatSize(nim_workmem_current()))

    set_cacheable(false, 1.0f, 0.0f, 0.0f)
    (res.status, res.content_type, res.content)

proc on_post(url: string, arg: string, ctype: string, buffer: string): (int, string, string) {.cdecl} =
    (200, ctype, buffer)

echo "Hello World!"
set_get_handler(on_get)
set_post_handler(on_post)
