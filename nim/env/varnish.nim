{.emit: """
#include "kvm_api.h"
""".}
proc backend_response*(status: int, ctype: cstring, clen: int, data: cstring, length: int) {.importc, header: "kvm_api.h"}
proc wait_for_requests*() {.importc, header: "kvm_api.h"}
proc sys_set_cacheable(cached: cint, ttl: cint, grace: cint, keep: cint) {.importc, header: "kvm_api.h"}
proc sys_http_append*(where: int, name: cstring, len: int) {.importc, header: "kvm_api.h"}
proc sys_http_set*(where: int, name: cstring, len: int) {.importc, header: "kvm_api.h"}
proc sys_http_find*(where: int, name: cstring, len: int, res: cstring, reslen: int): int {.importc, header: "kvm_api.h"}
proc sys_breakpoint*() {.importc, header: "kvm_api.h"}
type
    storage_cb* = proc(input: string): string {.cdecl.}
    storage_tramp = proc(callback: storage_cb, input: cstring, inplen: int) {.cdecl.}
    storage_task_cb* = proc(buffer: cstring, buflen: int) {.cdecl.}
proc nim_storage(callback: storage_cb, trampoline: storage_tramp, input: cstring, inplen: int, res: cstring, reslen: int): int {.importc.}
proc nim_storage_task(callback: storage_task_cb, argument: cstring, arglen: int, start: float32, period: float32): int {.importc.}
proc nim_finish_storage(res: cstring, reslen: int) {.importc.}

type
    get_cb = proc(url: string, arg: string): (int, string, string) {.cdecl}
    post_cb = proc(url: string, arg: string, ctype: string, data: string): (int, string, string) {.cdecl}
    error_cb = proc(url: string, arg: string, exception: string): (int, string, string) {.cdecl}

proc set_get_handler*(handler: get_cb) {.importc.}
proc set_post_handler*(handler: post_cb) {.importc.}
proc set_error_handler*(handler: error_cb) {.importc.}

proc backend_get_trampoline*(callback: get_cb, url: cstring, arg: cstring) {.exportc.} =
    let (status, a, b) = callback($url, $arg)
    backend_response(status, cstring(a), len(a), cstring(b), len(b))

#template ptrLenToOpenArray*[T](p: ptr T, s: int): auto =
#    toOpenArray(cast[ptr array[10000000, T]](p)[], 0, s - 1)

proc backend_post_trampoline*(callback: post_cb, url: cstring, arg: cstring, ctype: string, data: cstring, length: int) {.exportc.} =
    var postdata = newString(length)
    copyMem(addr(postdata[0]), data, length)
    let (status, a, b) = callback($url, $arg, $ctype, postdata)
    backend_response(status, cstring(a), len(a), cstring(b), len(b))

proc backend_error_trampoline*(callback: error_cb, url: cstring, arg: cstring, error: cstring) {.exportc.} =
    let (status, a, b) = callback($url, $arg, $error)
    backend_response(status, cstring(a), len(a), cstring(b), len(b))

proc response*(status: int, x: string, y: string) {.exportc.} =
    backend_response(status, x, len(x), y, len(y))

proc set_cacheable*(cached: bool, ttl: float, grace: float, keep: float) =
    sys_set_cacheable(cint(cached), cint(ttl * 1000.0), cint(grace * 1000.0), cint(keep * 1000.0))

let REQ:  int = 0
let RESP: int = 1

type
    Http* = object

proc get*(T: typedesc[Http], name: string): string =
    result = newString(4096)
    var size =
        sys_http_find(REQ, cstring(name), len(name), cstring(result), len(result))
    if size > 0:
        result.setLen(size)
    else:
        return ""

proc append*(T: typedesc[Http], header: string) =
    sys_http_append(RESP, cstring(header), len(header))

proc set*(T: typedesc[Http], header: string) =
    sys_http_set(RESP, cstring(header), len(header))

### Storage VM ###

proc storage_trampoline(callback: storage_cb, inpstr: cstring, inplen: int) {.cdecl.} =
    var input = newString(inplen)
    if inplen > 0:
        copyMem(addr(input[0]), inpstr, inplen)
    let data = callback(input)
    nim_finish_storage(cstring(data), len(data))
proc storage*(cb: storage_cb, input: string = ""): string =
    var res = newString(4096)
    let len = nim_storage(cb, storage_trampoline, cstring(input), len(input), cstring(res), len(res))
    if len >= 0:
        res.setLen(len)
        return res
    else:
        return ""

proc storage_task*(cb: storage_task_cb, argument: string = "", start: float = 0.0, period: float = 0.0) =
    discard nim_storage_task(cb, cstring(argument), len(argument), start, period)

### Memory and Stuff ###

proc nim_workmem_current*(): int {.importc.}
proc nim_workmem_max*(): int {.importc.}

### cURL ###

proc curl_fetch_wrapper(url: cstring, urllen: int) {.importc, header: "http.h".}
proc curl_post_wrapper(url: cstring, urllen: int, ct: cstring, ctlen: int, data: cstring, datalen: int) {.importc, header: "http.h".}
proc curl_get_status(): int {.importc, header: "http.h".}
proc curl_get_content_length(): cint {.importc, header: "http.h".}
proc curl_get_content_type_length(): cint {.importc, header: "http.h".}
proc curl_get_content(): cstring {.importc, header: "http.h".}
proc curl_get_content_type(): cstring {.importc, header: "http.h".}

type
    Curl* = object
        status*: int
        content*: string
        content_type*: string

proc fetch*(T: typedesc[Curl], url: string): Curl =
    # TODO: throw exception?
    curl_fetch_wrapper(url, len(url))
    let CL = curl_get_content_length()
    let CTL = curl_get_content_type_length()
    result.status = curl_get_status();
    result.content = newString(CL)
    if CL > 0:
        copyMem(addr(result.content[0]), curl_get_content(), CL)
    result.content_type = newString(CTL)
    if CTL > 0:
        copyMem(addr(result.content_type[0]), curl_get_content_type(), CTL)
    return result

proc post*(T: typedesc[Curl], url: string, ctype: string, data: string): Curl =
    # TODO: throw exception?
    curl_post_wrapper(url, len(url), ctype, len(ctype), data, len(data))
    let CL = curl_get_content_length()
    let CTL = curl_get_content_type_length()
    result.status = curl_get_status();
    result.content = newString(CL)
    if CL > 0:
        copyMem(addr(result.content[0]), curl_get_content(), CL)
    result.content_type = newString(CTL)
    if CTL > 0:
        copyMem(addr(result.content_type[0]), curl_get_content_type(), CTL)
    return result

### Debugging ###

proc breakpoint*() =
    sys_breakpoint()
