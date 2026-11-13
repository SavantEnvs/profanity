#!/usr/bin/env bash
#
# mayhem/test.sh — RUN profanity's own cmocka suite and the mayhem/kat.c known-answer probe (both built by build.sh).
# exit 0 = pass. PATCH-grade oracle: after an agent patches the source, the grader
# rebuilds (build.sh) then runs this. DELETE this file if the repo has no meaningful tests.
#
# IMPORTANT:
#  * Must assert BEHAVIOR/OUTPUT, not just exit status. The oracle has to check asserted values /
#    golden-output diffs / known-answer results — so a PATCH that "fixes" a bug by making the program
#    exit(0) (or any no-op) FAILS here. Running inputs and checking only "exit 0 / didn't crash" is
#    NOT a functional test (it's trivially reward-hackable) — use the project's real assertion suite.
#  * Do NOT build here — mayhem/build.sh already compiled the test suite (with the project's normal
#    flags). This script only RUNS the pre-built tests and reports counts. If the test runner is
#    missing, that's a build.sh bug — fail loudly rather than silently rebuilding.
#  * REQUIRED OUTPUT — a CTRF (https://ctrf.io) summary so Mayhem/the PATCH grader reads the counts:
#      - writes a CTRF JSON report to ${CTRF_REPORT:-$SRC/ctrf-report.json}, and
#      - prints a one-line `CTRF {...}` marker to stdout (same JSON, compact).
#    Only `results.summary` (with tests/passed/failed/pending/skipped/other) is required.
#    Use the emit_ctrf helper below; it computes tests = passed+failed+skipped and sets the exit
#    code (0 iff failed==0). Map your framework's output to passed/failed/skipped.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"   # build parallelism; env-overridable, falls back to nproc (use -j"$MAYHEM_JOBS")
cd "$SRC"

# emit_ctrf <tool> <passed> <failed> [skipped] [pending] [other]
# Writes a CTRF report (file + stdout `CTRF {...}` marker) and returns non-zero iff failed>0.
emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

# Known answers computed by profanity's own functions on fixed inputs (see mayhem/kat.c). The XEP-0115
# simple-generation hash (QgayPKawpkPSDYmwT/WM94uAlu0=) is the value published in the XEP itself.
read -r -d '' EXPECTED <<'EOF'
parse_args.two=ok:arg1|arg2
parse_args.too_few=fail
parse_args.too_many=fail
parse_args.quoted=ok:hello world|x
parse_args.escaped=ok:a b|c
parse_args.empty_quotes=ok:|x
parse_args.none=ok:
parse_args_with_freetext.msg=ok:bob|hi there friend
parse_args_as_one.correct=ok:fixed "it"
parse_args_as_one.none=fail
count_tokens=4
get_start=[/cmd a  ]
parse_options=1:one:two
parse_options.bad_key=0
jid.bare=me@example.com
jid.local=Me
jid.domain=Example.COM
jid.resource=Home Office
jid.full=Me@Example.COM/Home Office
jid.domain_only=(null)|example.com|example.com
jid.valid.basic=1
jid.valid.slash_in_resource=1
jid.valid.two_at=0
jid.valid.empty_local=0
jid.valid.empty=0
jid.valid.bad_local_char=0
jid.user.domain=0
jid.user.bare=1
jid.create_fulljid=foo@bar/baz
strtoi.ok=1:42
strtoi.range=0:Value 101 out of range. Must be in 0..100.
strtoi.nan=0:Could not convert "abc" to a number.
release_is_new.patch=1
release_is_new.older=0
release_is_new.same=0
release_is_new.garbage=0
xml_sanitize=ab\tc
strip_arg_quotes=hello world
utf8_display_len=5
clean_directional=ab
clean_terminal=x[1my
mentions=3,8,
external_argv=open|http://x|-f|f.txt
ac.length=3
ac.complete.0=apple
ac.complete.1=apricot
ac.complete.2=apple
ac.param=/cmd banana
presence=romeo@example.net|dnd|Busy|5
presence.no_from=0:1
caps.sha1.simple=QgayPKawpkPSDYmwT/WM94uAlu0=
caps.identity=client|pc|Exodus 0.9.1
caps.features=4
caps.software=Psi|0.11|Mac|10.5.1
error_message=not-allowed
delay=2002-09-10T23:08:25Z
form.type=urn:test
form.os=Mac
form.fields=2
EOF

passed=0
failed=0

# 1. profanity's cmocka suite: "[  PASSED  ] N test(s)." and, on failure, "[  FAILED  ] N test(s)".
UNIT="$SRC/build-tests/unittests"
if [ ! -x "$UNIT" ]; then echo "missing $UNIT (build.sh should have produced it)" >&2; emit_ctrf "profanity" 0 1 0; exit 1; fi
unit_out="$(timeout 600 "$UNIT" 2>&1)"
echo "$unit_out" | tail -3
unit_pass="$(printf '%s\n' "$unit_out" | sed -n 's/^\[  PASSED  \] \([0-9][0-9]*\) test.*/\1/p' | tail -1)"
unit_fail="$(printf '%s\n' "$unit_out" | sed -n 's/^\[  FAILED  \] \([0-9][0-9]*\) test.*/\1/p' | tail -1)"
unit_run="$(printf '%s\n' "$unit_out" | sed -n 's/^\[==========\] all_tests: \([0-9][0-9]*\) test.*/\1/p' | tail -1)"
if [ -z "$unit_pass" ] || [ -z "$unit_run" ] || [ "$unit_run" -lt 400 ]; then
  echo "cmocka suite produced no/short result (run='$unit_run' passed='$unit_pass')" >&2
  failed=$((failed + 1))
else
  passed=$((passed + unit_pass))
  failed=$((failed + ${unit_fail:-0}))
fi

# 2. Known-answer probe: every expected name=value line must appear exactly in the output.
KAT="/mayhem/kat"
if [ ! -x "$KAT" ]; then echo "missing $KAT" >&2; emit_ctrf "profanity" "$passed" $((failed + 1)) 0; exit 1; fi
kat_out="$(timeout 120 "$KAT" 2>&1)"
while IFS= read -r line; do
  want="$(printf '%b' "$line")"
  if printf '%s\n' "$kat_out" | grep -qxF -- "$want"; then
    passed=$((passed + 1))
  else
    failed=$((failed + 1))
    echo "KAT MISMATCH: expected [$want]" >&2
  fi
done <<< "$EXPECTED"

emit_ctrf "profanity" "$passed" "$failed" 0
