#!/usr/bin/env bash
set -e

mkdir -p .build
pushd .build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
popd
