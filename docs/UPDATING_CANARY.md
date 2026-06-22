# Updating to a newer Xenia-Canary

The whole project is structured so this is a small, repeatable chore. The engine lives as a
pinned submodule and our edits to it are minimal and logged in `CANARY_PATCHES.md`.

## Where our Canary edits live

Our Android-enabling edits to Canary are maintained as a branch (`android`) on the fork
`r0mn-creator/xenia-canary-fork` = *upstream Canary + our small patches on top*. The
`Xenia-Android` repo references that branch via the `third_party/xenia-canary` submodule.

## Procedure

1. **See what we changed.** From the repo root:
   ```
   grep -rn "XENIA-ANDROID:" third_party/xenia-canary/
   ```
   Cross-check against `docs/CANARY_PATCHES.md`.

2. **Rebase our patches onto new upstream.**
   ```
   cd third_party/xenia-canary
   git fetch upstream                      # upstream = xenia-canary/xenia-canary
   git checkout android
   git rebase upstream/canary_experimental # resolve conflicts ONLY in patched files
   git push origin android --force-with-lease
   ```

3. **Bump the submodule pointer** in the `Xenia-Android` repo and commit:
   ```
   cd ../..               # repo root
   git add third_party/xenia-canary
   git commit -m "Update Canary engine to <new-sha>"
   ```

4. **Rebuild and test** (see `BUILD.md`).

## If a patch conflicts

Each conflict is in one of the files listed in `CANARY_PATCHES.md`. Read that entry — it
explains *why* the edit exists — then re-apply the intent against the new upstream code.
Most edits are guarded with `#if XE_PLATFORM_ANDROID` / `if(ANDROID)`, so they rarely clash.

## Reducing future work to zero

The endgame is to get our Android support **merged upstream** into Canary. Every patch we
keep small, guarded, and upstreamable moves us toward "update = just bump the submodule, no
patches to re-apply."
