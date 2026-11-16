#!/usr/bin/env bash
# Captures every scenario of the given variants (default v2 v3 v4) into <out>/<variant>/.
# Build first: pio run -e treklink-screencap-v2 -e treklink-screencap-v3 -e treklink-screencap-v4
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
out="${OUT:-$here/shots}"
# A made-up fix on a trail near Sa Pa, Lao Cai, so position screens show a GPS lock. GPS="" turns it off.
gps="${GPS-22.3364,103.8438,1600}"
state="$(mktemp -d)"
trap 'rm -rf "$state"' EXIT
variants=("$@")
[ ${#variants[@]} -gt 0 ] || variants=(v2 v3 v4)
for v in "${variants[@]}"; do
  run() { python3 "$here/screencap.py" --variant "$v" --boot-wait 900 --out "$out/$v" ${gps:+--gps "$gps"} "$@"; }
  run --scenario "$here/scenarios/first-boot-$v.txt" --save-state "$state/$v"
  for s in "$here"/scenarios/frames-"$v".txt "$here"/scenarios/menu-frame-*-"$v".txt "$here"/scenarios/sos-"$v".txt "$here"/scenarios/compass-"$v".txt; do
    [ -f "$s" ] || continue
    run --scenario "$s" --load-state "$state/$v"
  done
done
