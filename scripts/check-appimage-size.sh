#!/usr/bin/env bash
# check-appimage-size.sh - fail the build if a watch app image exceeds the Pebble ceiling.
#
# Pebble caps a single app image's VIRTUAL_SIZE (static .text + .data + .bss, i.e. the
# ELF's LOAD segment MemSiz) at 65,535 bytes. Resources do NOT count. Going over means
# the watch silently refuses to load the app. Clock Dude draws everything from
# primitives and sits well under, so this is a tripwire rather than a tight budget.
#
# Usage: check-appimage-size.sh [path/to/pebble-app.elf ...]
#   with no arguments, checks every build/*/pebble-app.elf
set -euo pipefail

CEILING=65535
WARN_HEADROOM=4096

ELVES=("$@")
if [[ ${#ELVES[@]} -eq 0 ]]; then
  shopt -s nullglob
  ELVES=(build/*/pebble-app.elf)
  shopt -u nullglob
fi
if [[ ${#ELVES[@]} -eq 0 ]]; then
  echo "check-appimage-size: no ELF given and none found under build/" >&2
  exit 2
fi

# Any readelf can read the ELF's program headers cross-arch. Prefer the arm one,
# then a generic readelf (present on CI runners), then a local macOS Pebble SDK.
READELF="$(command -v arm-none-eabi-readelf || command -v readelf || true)"
if [[ -z "$READELF" ]]; then
  for cand in "$HOME/Library/Application Support/Pebble SDK/SDKs"/*/toolchain/arm-none-eabi/bin/arm-none-eabi-readelf; do
    [[ -x "$cand" ]] && READELF="$cand" && break
  done
fi
if [[ -z "$READELF" ]]; then
  echo "check-appimage-size: no arm-none-eabi-readelf on PATH or in the local Pebble SDK" >&2
  exit 2
fi

status=0
for elf in "${ELVES[@]}"; do
  if [[ ! -f "$elf" ]]; then
    echo "check-appimage-size: ELF not found: $elf" >&2
    exit 2
  fi

  # First LOAD program header; column 6 is MemSiz (hex, e.g. 0x04aee).
  memsize_hex="$("$READELF" -l "$elf" | awk '/LOAD/ {print $6; exit}')"
  if [[ -z "$memsize_hex" ]]; then
    echo "check-appimage-size: could not read LOAD MemSiz from $elf" >&2
    exit 2
  fi
  memsize=$(( memsize_hex ))   # bash parses 0x.. hex directly
  headroom=$(( CEILING - memsize ))

  printf '%-40s %6d bytes  (ceiling %d, headroom %d)\n' "$elf" "$memsize" "$CEILING" "$headroom"

  if (( memsize >= CEILING )); then
    echo "::error::$elf is at or over the ${CEILING}-byte app-image ceiling; the watch will not load it." >&2
    status=1
  elif (( headroom < WARN_HEADROOM )); then
    echo "::warning::$elf has only ${headroom} bytes of app-image headroom (under ${WARN_HEADROOM})."
  fi
done

exit "$status"
