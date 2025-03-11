#!/usr/bin/env bash
set -e
source="${1:-hello_world.c}"
tenant="test.com"
key="123"
host="127.0.0.1:8080"

file=$(mktemp /tmp/abc-script.XXXXXX)
gcc -static -O2 $source -o "$file"

curl -H "X-LiveUpdate: $key" -H "Host: $tenant" --data-binary "@$file" -X POST $host

rm -f "$file"
