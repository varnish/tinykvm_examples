#!/usr/bin/env bash
set -e
mkdir -p .build
pushd .build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja
popd

file=".build/gbcemu"
tenant="gbc"
key="123"
host="http://127.0.0.1:8080/update"

set -x
curl -D - -H "X-LiveUpdate: $key" -H "Host: $tenant" --data-binary "@$file" -X POST $host
