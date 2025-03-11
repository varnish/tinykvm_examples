package main

import (
	"bytes"
	"fmt"
	"image"
	"io"
	"log"
	"net/http"
	"os"

	_ "image/jpeg"
	"image/png"
	_ "image/png"

	"varnish"

	"github.com/Kagami/go-avif"
)

var AvifEncoding = "true"

func main() {
	if varnish.IsLinuxMain() {
		local_transcoding()
		return
	}

	varnish.OnBackendGet(func(url string, conf string) {

		varnish.HttpSet("X-Go: GET")

		// Fetch asset from provided URL
		resp := varnish.Fetch(url)
		if resp.Status != 200 {
			varnish.Deliver(500, "text/plain", []byte("Failed to fetch image asset"))
		}

		// Decode JPEG or other image format
		content_reader := bytes.NewReader(resp.Content)
		img, _, err := image.Decode(content_reader)
		if err != nil {
			varnish.HttpSet("X-Error: Failed to decode image")
			varnish.Deliver(200, resp.ContentType, resp.Content)
		}

		var buf bytes.Buffer
		bufwriter := io.Writer(&buf)

		if AvifEncoding == "true" {
			// Encode to AVIF (fast settings)
			avifOpts := avif.Options{
				Threads: 1,
				Speed:   8,
				Quality: 25,
			}
			if err := avif.Encode(bufwriter, img, &avifOpts); err != nil {
				varnish.HttpSet("X-Error: Failed to encode AVIF image")
				varnish.Deliver(200, resp.ContentType, resp.Content)
			}
		} else {
			// Encode to PNG
			if err := png.Encode(bufwriter, img); err != nil {
				varnish.HttpSet("X-Error: Failed to encode PNG image")
				varnish.Deliver(200, resp.ContentType, resp.Content)
			}
		}

		// Fetch rose.jpg asset
		varnish.Deliver(200, "image/avif", buf.Bytes())
	})

	varnish.OnBackendPost(func(url string, conf string, content_type string, content []byte) {

		varnish.HttpSet("X-Go: POST")

		// Decode JPEG or other image format
		content_reader := bytes.NewReader(content)
		img, _, err := image.Decode(content_reader)
		if err != nil {
			varnish.HttpSet("X-Error: Failed to decode image")
			varnish.Deliver(200, content_type, content)
		}

		var buf bytes.Buffer
		bufwriter := io.Writer(&buf)

		if AvifEncoding == "true" {
			// Encode to AVIF (fast settings)
			avifOpts := avif.Options{
				Threads: 1,
				Speed:   8,
				Quality: 25,
			}
			if err := avif.Encode(bufwriter, img, &avifOpts); err != nil {
				varnish.HttpSet("X-Error: Failed to encode AVIF image")
				varnish.Deliver(200, content_type, content)
			}
		} else {
			// Encode to PNG
			if err := png.Encode(bufwriter, img); err != nil {
				varnish.HttpSet("X-Error: Failed to encode PNG image")
				varnish.Deliver(200, content_type, content)
			}
		}

		// Fetch rose.jpg asset
		varnish.Deliver(200, "image/avif", buf.Bytes())
	})

	fmt.Println("Go AVIF transcoder ready")
	varnish.WaitForRequests()
}

// This fetches rose.jpg from local Varnish, transcodes it to AVIF and writes
// it to a file for inspection.
func local_transcoding() {

	resp, err := http.Get("http://127.0.0.1:8080/avif/image")
	if err != nil {
		log.Fatalln(err)
	}

	img, _, err := image.Decode(resp.Body)
	if err != nil {
		log.Fatalln(err)
	}

	var buf bytes.Buffer
	bufwriter := io.Writer(&buf)

	if AvifEncoding == "true" {
		// Encode to AVIF (fast settings)
		avifOpts := avif.Options{
			Threads: 1,
			Speed:   8,
			Quality: 25,
		}
		if err := avif.Encode(bufwriter, img, &avifOpts); err != nil {
			log.Fatalln(err)
		}
	} else {
		if err := png.Encode(bufwriter, img); err != nil {
			log.Fatalln(err)
		}
	}

	fmt.Println("Transcoding complete")

	err = os.WriteFile("rose.avif", buf.Bytes(), 0644)
	if err != nil {
		log.Fatal(err)
	}
}
