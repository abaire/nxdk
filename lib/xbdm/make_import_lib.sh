#!/usr/bin/env bash
set -eu
set -o pipefail

llvm-dlltool -m i386 -d xbdm.dll.def -l libxbdm.lib
