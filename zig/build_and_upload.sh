#!/usr/bin/env bash
set -e
source="${1:-example}"
tenant="test.com"
key="123"
host="127.0.0.1:8080"

file=$source
zig build-exe -isystem . $source.zig

curl -H "X-LiveUpdate: $key" -H "Host: $tenant" --data-binary "@$file" -X POST $host
