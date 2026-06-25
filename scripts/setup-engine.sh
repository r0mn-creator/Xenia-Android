#!/usr/bin/env bash
#
# Fetches the pinned Xenia-Canary engine and applies the Xenia Android patches.
#
# The engine is large upstream BSD source, so it is not committed to this repo.
# Instead we pin it to a known-good commit and keep our changes as a single,
# documented patch (see docs/CANARY_PATCHES.md). Run this once before building.
#
set -euo pipefail

# Pinned Xenia-Canary commit this project is built against.
CANARY_COMMIT="0b4e6c4"
CANARY_URL="https://github.com/xenia-canary/xenia-canary.git"

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ENGINE_DIR="$ROOT/third_party/xenia-canary"
PATCH="$ROOT/patches/xenia-android-canary.patch"

echo ">> Xenia Android — engine setup"
echo "   commit: $CANARY_COMMIT"
echo "   dir:    $ENGINE_DIR"

if [ ! -d "$ENGINE_DIR/.git" ]; then
  echo ">> Cloning Xenia-Canary..."
  git clone "$CANARY_URL" "$ENGINE_DIR"
fi

cd "$ENGINE_DIR"
echo ">> Checking out pinned commit + submodules..."
git fetch --all --tags
git checkout -- . 2>/dev/null || true   # drop any previously-applied patch
git checkout "$CANARY_COMMIT"
git submodule update --init --recursive

echo ">> Applying Xenia Android engine patches..."
git apply --whitespace=nowarn "$PATCH"

echo ">> Done. Engine ready (Canary @ $CANARY_COMMIT + Xenia Android patches)."
echo "   Now build with:  ./gradlew :app:assembleDebug"
