#!/usr/bin/env bash
set -e
cargo build --release --target=x86_64-unknown-linux-musl

file="target/x86_64-unknown-linux-musl/release/collector"
tenant="test.com"
key="123"
host="127.0.0.1:8080"
curl -H "X-LiveUpdate: $key" -H "Host: $tenant" --data-binary "@$file" -X POST $host
