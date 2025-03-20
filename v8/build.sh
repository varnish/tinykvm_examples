#!/bin/bash

set -e

mkdir -p .build
pushd .build
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo
make -j8
popd

