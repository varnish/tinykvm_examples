package varnish

// #cgo CFLAGS: -O2 -g -Wall -Wno-unused-variable
// #cgo LDFLAGS: -static
// #include "goapi.h"
import "C"
import (
	"errors"
	"os"
	"strings"
	"unsafe"
)

func IsLinuxMain() bool {
	return strings.Contains(os.Args[0], "/")
}

type GetHandler func(string, string)
type PostHandler func(string, string, string, []byte)

var get_handler GetHandler
var post_handler PostHandler

func OnBackendGet(fp GetHandler) {
	get_handler = fp
}
func OnBackendPost(fp PostHandler) {
	post_handler = fp
}

//export go_get_handler
func go_get_handler(url *C.char, arg *C.char) {
	get_handler(C.GoString(url), C.GoString(arg))
}

//export go_post_handler
func go_post_handler(url *C.char, arg *C.char, ctype *C.char, cont *C.void, clen C.size_t) {
	post_handler(C.GoString(url), C.GoString(arg), C.GoString(ctype), C.GoBytes(unsafe.Pointer(cont), C.int(clen)))
}

func WaitForRequests() {
	C.setup_varnish_for_go()
}

func Deliver(status int, ctype string, content []byte) {
	c_ctype := C.CString(ctype)
	C.go_backend_response(C.short(status),
		c_ctype, C.size_t(len(ctype)),
		(*C.uchar)(unsafe.Pointer(&content[0])), C.size_t(len(content)))
}

// Caching functions

func Set_cacheable(cached bool, ttl float32, grace float32, keep float32) {
	C.go_set_cacheable(C.bool(cached), C.float(ttl), C.float(grace), C.float(keep))
}

// HTTP functions

func HttpSet(field string) {
	C.sys_http_set(C.RESP, C.CString(field), C.size_t(len(field)))
}

// Fetch functions

type FetchResponse struct {
	Status      int
	ContentType string
	Content     []byte
}

func Fetch(url string) *FetchResponse {
	r := FetchResponse{}
	opres := C.struct_curl_op{}

	result :=
		C.sys_fetch(C.CString(url), C.size_t(len(url)), (*C.struct_curl_op)(unsafe.Pointer(&opres)), nil, nil)

	if result < 0 {
		r.Status = 500
	} else {
		r.Status = int(opres.status)
		r.ContentType = C.GoString((*C.char)(unsafe.Pointer(&opres.ctype)))
		r.Content = C.GoBytes(opres.content, C.int(opres.content_length))
	}

	return &r
}

// Storage functions
var storage_lookup = make(map[string]func([][]byte) []byte)

//export storage_trampoline
func storage_trampoline(n C.size_t, buffers *C.struct_virtbuffer, res C.size_t) {

	name_buffer := C.virtbuffer_at(buffers, 0)
	name := C.GoStringN((*C.char)(name_buffer.data), C.int(name_buffer.len))

	slices := make([][]byte, 1)
	arg_buffer := C.virtbuffer_at(buffers, 1)
	slices[0] =
		C.GoBytes(arg_buffer.data, C.int(arg_buffer.len))

	result := storage_lookup[name](slices)
	C.storage_return(unsafe.Pointer(&result[0]), C.size_t(len(result)))
}

//export storage_strings_trampoline
func storage_strings_trampoline(n C.size_t, buffers *C.struct_virtbuffer, res C.size_t) {

	// Create string-array from input buffers
	slices := make([][]byte, int(n)-1)
	for i := 1; i < int(n); i++ {
		var str_buffer = C.virtbuffer_at(buffers, C.size_t(i))
		slices[i-1] =
			C.GoBytes(str_buffer.data, C.int(str_buffer.len))
	}

	var name_buffer = C.virtbuffer_at(buffers, 0)
	var name = C.GoStringN((*C.char)(name_buffer.data), C.int(name_buffer.len))

	var result = storage_lookup[name](slices)
	C.storage_return(unsafe.Pointer(&result[0]), C.size_t(len(result)))
}

func StorageRegister(name string, fn func([][]byte) []byte) {
	storage_lookup[name] = fn
}

func StorageCall(fn string, data []byte) ([]byte, error) {
	var fnb = unsafe.StringData(fn)
	var fns = unsafe.Slice(fnb, len(fn))

	resmax := C.size_t(2 * 1024 * 1024)
	resbuf := C.malloc(resmax)
	defer C.free(unsafe.Pointer(resbuf))

	result :=
		C.go_storage_call(unsafe.Pointer(&fns[0]), C.size_t(len(fn)), unsafe.Pointer(&data[0]), C.size_t(len(data)), unsafe.Pointer(resbuf), resmax)
	if result < 0 {
		return nil, errors.New("storage call failed")
	}

	return C.GoBytes(resbuf, C.int(result)), nil
}

func StorageCallV(fn string, input [][]byte) ([]byte, error) {
	// Function name + []byte array
	n := len(input) + 1

	// Create array of virtbuffers
	cArray := C.malloc(C.size_t(n * 16))
	defer C.free(unsafe.Pointer(cArray))
	a := (*[1<<30 - 1]C.struct_virtbuffer)(cArray)

	// array[0] = storage function
	a[0].data = unsafe.Pointer(unsafe.StringData(fn))
	a[0].len = C.size_t(len(fn))

	// array[1..n] = []byte array
	for i, sl := range input {
		a[i+1].data = unsafe.Pointer(&sl[0])
		a[i+1].len = C.size_t(len(sl))
	}

	// Result buffer
	resmax := C.size_t(2 * 1024 * 1024)
	resbuf := C.malloc(resmax)
	defer C.free(unsafe.Pointer(resbuf))

	result :=
		C.go_execute_storage_call_strings((*C.struct_virtbuffer)(cArray), C.size_t(n), unsafe.Pointer(resbuf), resmax)
	if result < 0 {
		return nil, errors.New("storage call failed")
	}

	return C.GoBytes(resbuf, C.int(result)), nil
}
