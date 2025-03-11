#!/usr/bin/env bash
source="${1:-src/chat.js}"
set -e
mkdir -p .build
python3 static_builder.py www .build/static_site.c
pushd .build
cmake .. -DJSFILE=$source
make -j4
popd

file=".build/jsapp"
tenant="test.com"
key="123"
host="127.0.0.1:8080"

curl -H "X-LiveUpdate: $key" -H "Host: $tenant" --data-binary "@$file" -X POST $host
