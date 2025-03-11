#!/usr/bin/env bash
set -e
source="${1:-example}"
tenant="test.com"
key="123"
host="127.0.0.1:8080"

cfile=.build/output.c
binfile=.build/output.elf
mkdir -p .build
nelua --print-code $DMODE $source.nelua > $cfile

gcc -static -O2 -g3 -Wall -Wextra env/main.c $cfile -o $binfile

curl -H "X-LiveUpdate: $key" -H "Host: $tenant" --data-binary "@$binfile" -X POST $host
