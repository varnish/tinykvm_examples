#!/usr/bin/env bash
set -e
mkdir -p .build
pushd .build
cmake .. -G Ninja
ninja
strip --strip-all thumbnails
popd

file=".build/thumbnails"
tenant="test.com"
key="123"
host="127.0.0.1:8080"

curl -H "X-LiveUpdate: $key" -H "Host: $tenant" --data-binary "@$file" -X POST $host
