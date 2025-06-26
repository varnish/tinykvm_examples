#!/bin/bash

set -e
$CC -Wall -Wextra -O2 -o libvdeno.so varnish-deno.c \
	-I$PWD \
	-shared -fPIC \
	-Wl,--export-dynamic
