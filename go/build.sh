#!/usr/bin/env bash
set -e

pushd goexample
go build
popd

# Why do we need workarounds for everything? Why??
export CGO_LDFLAGS="-static -laom -lm"
pushd avif
go build
popd
