#!/bin/bash

set -e

mkdir -p .build
pushd .build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j8
popd

