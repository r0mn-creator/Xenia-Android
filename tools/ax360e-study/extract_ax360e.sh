#!/usr/bin/env bash
# Extracts XE_PLATFORM_AX360E #if/#ifdef blocks (with a few lines of context) from a file.
# Usage: extract_ax360e.sh <file>
AX=/home/roman/Android/ax360e/app/src/main/cpp/xenia-canary
f="$AX/src/xenia/$1"
[ -f "$f" ] || { echo "missing: $f"; exit 1; }
awk '
  /XE_PLATFORM_AX360E/ { print NR": "$0; ctx=1; depth=0; next }
  ctx { print NR": "$0;
        if ($0 ~ /#if/) depth++;
        if ($0 ~ /#endif/) { if (depth==0) {ctx=0; print "---"} else depth-- } }
' "$f"
