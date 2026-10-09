#!/bin/sh
# Builds SpyroFOVHotkeys.asi with zig (https://ziglang.org). Usage: ZIG=/path/to/zig ./build.sh
set -e
ZIG=${ZIG:-zig}
ZIGLIB=$(dirname "$(command -v "$ZIG")")/lib
"$ZIG" cc -target x86_64-windows-gnu -O2 -Wall -Wextra -shared -s \
  -nostdlib -fno-builtin -ffreestanding -fno-stack-protector -fno-sanitize=all \
  -isystem "$ZIGLIB/libc/include/any-windows-any" \
  -Wl,-e,DllMain -o SpyroFOVHotkeys.asi fovkeys.c -lkernel32 -luser32
