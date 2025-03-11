import varnish
# -= storage.nim =-
# This program lets you store something in the
# mutable storage using a POST request. Mutable
# storage is separate from request VMs, and has
# the ability to retain its state across updates
# and even Varnish restarts.
#

var stored_value: string = "Value stored in storage"

proc storage_set_value(input: string): string {.cdecl.} =
    # This code is running inside mutable storage
    stored_value = input
    return stored_value

proc storage_get_value(_: string): string {.cdecl.} =
    # This code is running inside mutable storage
    return stored_value

proc on_get(url: string, arg: string): (int, string, string) {.cdecl} =
    set_cacheable(false, 1.0f, 0.0f, 0.0f)
    #breakpoint()
    # Retrieve value from mutable storage
    let value = storage(storage_get_value)
    (200, "text/plain", value)

proc on_post(url: string, arg: string, ctype: string, data: string): (int, string, string) {.cdecl} =
    # Retrieve value from mutable storage, with POST data as input
    let value = storage(storage_set_value, data)
    (200, ctype, value)

set_get_handler(on_get)
set_post_handler(on_post)
