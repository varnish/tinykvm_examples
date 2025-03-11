#!/usr/bin/env bash
set -e
tenant="test.com"
key="123"
host="127.0.0.1:8080"

for i in {1..500}
do
    curl -H "X-LiveUpdate: $key" -H "Host: $tenant" --data-binary "@$1" -X POST $host
done
