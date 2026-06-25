#!/usr/bin/env bash
# Diff a source file between OUR build and aX360e. Usage: diff_vs_ax360e.sh <relpath under src/xenia/>
AX=/home/roman/Android/ax360e/app/src/main/cpp/xenia-canary/src/xenia
OUR=/home/roman/Android/Xenia-Android/third_party/xenia-canary/src/xenia
diff -u "$OUR/$1" "$AX/$1" 2>/dev/null | head -200
