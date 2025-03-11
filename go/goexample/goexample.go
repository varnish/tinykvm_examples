package main

import (
	"bytes"
	"fmt"
	"strings"

	"varnish"
)

var storage_data []string

func my_storage(data [][]byte) []byte {
	strdata := bytes.Join(data, []byte(" "))
	storage_data = append(storage_data, string(strdata))

	all := strings.Join(storage_data, "\n")
	result := "Yep, it works! Storage:\n" + all
	return []byte(result)
}

func main() {
	if varnish.IsLinuxMain() {
		return
	}

	varnish.OnBackendGet(func(url string, conf string) {

		varnish.HttpSet("X-Go: GET")

		res1, err := varnish.StorageCall("my_storage", []byte("Some input byte data"))
		if err != nil {
			varnish.Deliver(500, "text/plain", []byte("Storage call []byte failed"))
		}

		res2, err := varnish.StorageCallV("my_storage",
			[][]byte{[]byte("Some"), []byte("Data"), []byte("For"), []byte("Storage"), []byte("!")})
		if err != nil {
			varnish.Deliver(500, "text/plain", []byte("Storage call []string failed"))
		}

		var b = append(append(res1, []byte("\n")...), res2...)
		varnish.Deliver(200, "text/plain", b)
	})

	varnish.OnBackendPost(func(url string, conf string, content_type string, content []byte) {

		varnish.HttpSet("X-Go: POST")

		varnish.Deliver(200, content_type, content)
	})

	varnish.StorageRegister("my_storage", my_storage)

	fmt.Println("Go Compute Example ready")
	varnish.WaitForRequests()
}
