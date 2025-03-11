#!/usr/bin/env bash
set -e
mkdir -p .build
pushd .build
cmake .. -G Ninja
ninja
strip --strip-all sqlite
popd
