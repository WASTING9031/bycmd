#!/bin/sh
set -eu
cd "$(dirname "$0")"
if [ -n "${NDK_BUILD:-}" ]; then exec "$NDK_BUILD" "$@"; fi
if [ -n "${ANDROID_NDK_HOME:-}" ] && [ -x "$ANDROID_NDK_HOME/ndk-build" ]; then exec "$ANDROID_NDK_HOME/ndk-build" "$@"; fi
if command -v ndk-build >/dev/null 2>&1; then exec ndk-build "$@"; fi
AIDE=/data/user/0/com.aide.clone/no_backup/ndksupport-1710240003/android-ndk-aide/ndk-build
if [ -x "$AIDE" ]; then exec "$AIDE" "$@"; fi
echo 'Android NDK not found. Set ANDROID_NDK_HOME or NDK_BUILD.' >&2; exit 1
