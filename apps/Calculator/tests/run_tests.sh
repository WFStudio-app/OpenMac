#!/usr/bin/env bash
# tests/run_tests.sh — Calculator.app self-tests (OpenMac, proprietary)
set -uo pipefail
cd "$(dirname "$0")/.."

BIN=./calculator
PASS=0; FAIL=0

check() { # desc expected actual
  if [[ "$2" == "$3" ]]; then PASS=$((PASS+1)); else
    FAIL=$((FAIL+1)); echo "FAIL: $1 | expected [$2] got [$3]"; fi
}

[[ -x "$BIN" ]] || { echo "build the binary first (make)"; exit 1; }

check "basic add"        "4"   "$($BIN -e '2+2')"
check "precedence"       "6"   "$($BIN -e '2+2*2')"
check "parens"           "8"   "$($BIN -e '(2+2)*2')"
check "div"              "0.5" "$($BIN -e '1/2')"
check "modulo"           "1"   "$($BIN -e '7%2')"
check "power right-assoc" "512" "$($BIN -e '2^9')"
check "power assoc"      "512" "$($BIN -e '2^3^2')"     # 2^(3^2)=512
check "unary minus"      "-4"  "$($BIN -e '-(2+2)')"
check "double neg"       "5"   "$($BIN -e '--5')"
check "sqrt"             "3"   "$($BIN -e 'sqrt(9)')"
check "cbrt"             "3"   "$($BIN -e 'cbrt(27)')"
check "abs"              "5"   "$($BIN -e 'abs(-5)')"
check "fact"             "120" "$($BIN -e 'fact(5)')"
check "round"            "3"   "$($BIN -e 'round(2.5)')"
check "floor"            "2"   "$($BIN -e 'floor(2.9)')"
check "ceil"             "3"   "$($BIN -e 'ceil(2.1)')"
check "log base10"       "2"   "$($BIN -e 'log(100)')"
check "ln e"             "1"   "$($BIN -e 'ln(e)')"
check "sin pi ~ 0"       "0"   "$($BIN -e 'round(sin(pi))')"
check "constant pi expr" "0"   "$($BIN -e 'round(sin(tau/2))*0')"
check "spaces tolerated" "4"   "$($BIN -e ' 2 + 2 ')"
check "bare arg form"    "6"   "$($BIN '2+2*2')"
check "multi-arg join"   "4"   "$($BIN -e 2 + 2)"

# error cases: nonzero exit + message on stderr
err_out=$($BIN -e '1/0' 2>&1); rc=$?
[[ $rc -ne 0 && "$err_out" == *"division by zero"* ]] \
  && PASS=$((PASS+1)) || { FAIL=$((FAIL+1)); echo "FAIL: div-by-zero ($rc) $err_out"; }

err_out=$($BIN -e 'sqrt(-1)' 2>&1); rc=$?
[[ $rc -ne 0 && "$err_out" == *"domain error"* ]] \
  && PASS=$((PASS+1)) || { FAIL=$((FAIL+1)); echo "FAIL: sqrt domain ($rc) $err_out"; }

err_out=$($BIN -e '2+' 2>&1); rc=$?
[[ $rc -ne 0 && "$err_out" == *"unexpected end"* ]] \
  && PASS=$((PASS+1)) || { FAIL=$((FAIL+1)); echo "FAIL: trailing op ($rc) $err_out"; }

err_out=$($BIN -e '(2+2' 2>&1); rc=$?
[[ $rc -ne 0 && "$err_out" == *"expected ')'"* ]] \
  && PASS=$((PASS+1)) || { FAIL=$((FAIL+1)); echo "FAIL: unclosed paren ($rc) $err_out"; }

err_out=$($BIN -e 'foo(1)' 2>&1); rc=$?
[[ $rc -ne 0 && "$err_out" == *"unknown identifier"* ]] \
  && PASS=$((PASS+1)) || { FAIL=$((FAIL+1)); echo "FAIL: unknown ident ($rc) $err_out"; }

# REPL smoke test incl. '_' last-answer and history command
repl_out=$(printf '2+3\n_*2\nhistory\nquit\n' | $BIN --sci 2>&1)
echo "$repl_out" | grep -q "= 5" \
  && PASS=$((PASS+1)) || { FAIL=$((FAIL+1)); echo "FAIL: repl basic"; }
echo "$repl_out" | grep -q "= 10" \
  && PASS=$((PASS+1)) || { FAIL=$((FAIL+1)); echo "FAIL: repl underscore"; }

# history persistence (isolated HOME so the test is reproducible)
export HOME="$(mktemp -d)"
$BIN -e '40+2' >/dev/null
$BIN --history 1 | grep -q "40+2 = 42" \
  && PASS=$((PASS+1)) || { FAIL=$((FAIL+1)); echo "FAIL: history show"; }
rm -rf "$HOME"

echo "Calculator tests: $PASS passed, $FAIL failed"
[[ $FAIL -eq 0 ]]
