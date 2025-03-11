#!/usr/bin/env bash
set -e
source="${1:-main.kt}"
tenant="test.com"
key="123"
host="127.0.0.1:8080"

file=$(mktemp /tmp/abc-script.XXXXXX)
source interop.sh # should only be run once
kotlinc-native -produce library varnish.kt -l libvarnish -o varnish
kotlinc-native -produce static $source -l varnish
$CC -std=gnu99 -O2 -c env/main.c -o $file.o
$CXX -O2 -static $file.o libstatic.a -fuse-ld=mold -pthread -ldl -o $file

curl -H "X-LiveUpdate: $key" -H "Host: $tenant" --data-binary "@$file" -X POST $host

rm -f "$file.o"
rm -f "$file"
