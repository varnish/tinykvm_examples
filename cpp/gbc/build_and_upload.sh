#!/usr/bin/env bash
set -e
mkdir -p .build
pushd .build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja
popd

file=".build/gbcemu"
tenant="gameboy.com"
key="12daf155b8508edc4a4b8002264d7494"
host="https://sandbox.varnish-software.com"

curl -H "X-PostKey: $key" -H "Host: $tenant" --data-binary "@$file" -X POST $host
