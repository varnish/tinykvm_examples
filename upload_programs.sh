#!/bin/bash
BIN=tinykvm_programs

upload() {
	base=`basename $1`
	curl -H "Host: filebin.varnish-software.com" --data-binary "@$1" -X POST https://filebin.varnish-software.com/$BIN/$base
}

upload_program() {
	base=`basename $1`
	tar -cJf - $1 | curl -H "Host: filebin.varnish-software.com" --data-binary "@-" -X POST https://filebin.varnish-software.com/$BIN/$base.tar.xz
}

upload_with_storage() {
	base=`basename $1`
	tar -cJf - $1 $2 | curl -H "Host: filebin.varnish-software.com" --data-binary "@-" -X POST https://filebin.varnish-software.com/$BIN/$base.tar.xz
}

pack_with_storage() {
	base=`basename $1`
	tar -cJf - $1 $2 | xz -9 > $base.tar.xz
}

#pack_with_storage cpp/minimal/minimal cpp/minimal/storage
#exit 0

upload compute.json
upload_program go/goexample/goexample
upload_program javascript/.build/jsapp
upload_program cpp/gbc/.build/gbcemu
#
upload_with_storage cpp/minimal/minimal cpp/minimal/storage
upload_with_storage cpp/collector/collector cpp/collector/storage
#
upload_program c/scounter
upload_program cpp/.build/basicauth
#
##upload_program rust/collector/target/x86_64-unknown-linux-musl/release/collector
#upload_program rust/png/target/x86_64-unknown-linux-musl/release/rustpng
#upload_program javascript/.build/jsapp
upload_program cpp/.build/espeak
upload_program cpp/.build/hello_world
upload_program cpp/.build/fetch
upload_program cpp/.build/minify
upload_program cpp/.build/to_string
upload_program cpp/base64/.build/base64pp
upload_program cpp/thumbnail/.build/thumbnails
upload_program cpp/xml/.build/xmlpp
upload_program cpp/zstd/.build/zstdpp
#
upload_program nim/prefetch_task
upload_program nim/storage_example
upload_program nim/watermark
#
upload_program cpp/avif/.build/avifencode
upload_program cpp/webp/.build/webpencoder
upload_program cpp/llama/.build/llamapp
upload_program cpp/sd/.build/sdpp
upload_program ~/github/Stockfish/src/stockfish
#
