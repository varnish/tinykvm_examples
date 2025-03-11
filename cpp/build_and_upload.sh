#!/usr/bin/env bash
source="${1:-src/hello_world.cpp}"
set -e
mkdir -p .build
pushd .build
cmake .. -G Ninja -DSRCFILE=$source
ninja
popd

base_fname=`basename -s .cpp $source`
mv .build/cpp_app .build/$base_fname
file=.build/$base_fname
ls -lah $file

#tenant="test.com"
#key="123"
#host="127.0.0.1:8080"
#curl -H "X-LiveUpdate: $key" -H "Host: $tenant" --data-binary "@$file" -X POST $host
