#!/usr/bin/env bash
set -e
project="${1-hello}"
#nimble install pixie jwt
nim c --app:staticLib -d:release --gc:arc --noMain --cincludes:env --path:env $project.nim
gcc -static -O2 -g3 -Wl,--whole-archive lib$project.a -Wl,--no-whole-archive -lm env/main.c env/http.c -o $project
rm -f lib$project.a

file="$project"
tenant="test.com"
key="123"
host="127.0.0.1:8080"
curl -H "X-LiveUpdate: $key" -H "Host: $tenant" --data-binary "@$file" -X POST $host
