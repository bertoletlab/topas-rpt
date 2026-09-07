#!/usr/bin/env bash
set -u
TOPAS_BIN="${TOPAS_BIN:-topas}"

if ! command -v "$TOPAS_BIN" >/dev/null 2>&1; then
  echo "ERROR: TOPAS binary not found. Set TOPAS_BIN=/path/to/topas or add to PATH."
  exit 2
fi

ROOT="$(cd "$(dirname "$0")" && pwd)"
PARAM_DIR="$ROOT/param"
RUN_DIR="$ROOT/runs"
mkdir -p "$RUN_DIR"

red() { printf "\033[31m%s\033[0m\n" "$*"; }
grn() { printf "\033[32m%s\033[0m\n" "$*"; }

fail_cnt=0
pass_cnt=0

for p in "$PARAM_DIR"/*.txt; do
  tname=$(basename "$p" .txt)
  exp="${p%.txt}.exp"

  echo "==> Running: $tname"
  rdir="$RUN_DIR/$tname"
  rm -rf "$rdir" && mkdir -p "$rdir"

  # run from the test run directory so outputs land here
  cp "$p" "$rdir/$tname.txt"
  ( cd "$rdir" && "$TOPAS_BIN" "$tname.txt" > "$tname.log" 2>&1 )
  rc=$?

  status="PASS"
  reason=""

  # exit code must be zero
  if [ $rc -ne 0 ]; then
    status="FAIL"
    reason="Exit code $rc"
  fi

  # no fatal errors in log
  if grep -Eiq 'TOPAS is exiting due to a serious error|FATAL ERROR' "$rdir/$tname.log"; then
    status="FAIL"
    reason="Fatal error in log"
  fi

  # all expected files must exist
  if [ -f "$exp" ]; then
    while IFS= read -r line; do
      case "$line" in
        FILE:*)
          f="${line#FILE:}"
          f="$(echo "$f" | xargs)"
          if [ ! -f "$rdir/$f" ]; then
            status="FAIL"
            reason="Missing expected file: $f"
            break
          fi
          ;;
      esac
    done < "$exp"
  fi

  if [ "$status" = "PASS" ]; then
    grn "PASS: $tname"
    pass_cnt=$((pass_cnt + 1))
  else
    red "FAIL: $tname - $reason"
    fail_cnt=$((fail_cnt + 1))
  fi
done

echo
echo "Summary: $pass_cnt passed, $fail_cnt failed"
[ $fail_cnt -eq 0 ] || exit 1