#!/usr/bin/env bash
# tests/run_tests.sh - Stickies.app test suite (OpenMac)
set -u
cd "$(dirname "$0")/.."

BIN=./stickies
export STICKIES_ROOT="$(mktemp -d)/Stickies"
export STICKIES_NO_COLOR=1

PASS=0; FAIL=0
ok()   { PASS=$((PASS+1)); echo "  ok: $1"; }
bad()  { FAIL=$((FAIL+1)); echo "FAIL: $1"; }

check() { # check <desc> <expected> <actual>
  if [[ "$2" == "$3" ]]; then ok "$1"; else bad "$1 (expected '$2', got '$3')"; fi
}

# 1. help
$BIN --help | head -1 | grep -q "Stickies.app" && ok "help banner" || bad "help banner"

# 2. empty list
check "empty list" "(no stickies)" "$($BIN list)"

# 3. new with -t
out=$($BIN new t1 -c pink -t "hello")
check "new message" "Created sticky 't1' (pink)." "$out"

# 4. file exists
[[ -f "$STICKIES_ROOT/notes/t1.text" ]] && ok "note file created" || bad "note file created"
[[ -f "$STICKIES_ROOT/notes/t1.meta"  ]] && ok "meta file created" || bad "meta file created"

# 5. content matches
check "content" "hello" "$(cat "$STICKIES_ROOT/notes/t1.text" | tr -d '\n')"

# 6. duplicate rejected
$BIN new t1 -t x >/dev/null 2>&1 && bad "duplicate rejected" || ok "duplicate rejected"

# 7. invalid name rejected
$BIN new "bad/name" -t x >/dev/null 2>&1 && bad "invalid name rejected" || ok "invalid name rejected"

# 8. unknown colour rejected
$BIN new t2 -c chartreuse >/dev/null 2>&1 && bad "unknown colour rejected" || ok "unknown colour rejected"

# 9. add appends
$BIN add t1 "world" >/dev/null
check "add appends" "helloworld" "$(cat "$STICKIES_ROOT/notes/t1.text" | tr -d '\n')"

# 10. show prints lines
lines=$($BIN show t1 | wc -l | tr -d ' ')
check "show line count" "2" "$lines"

# 11. list contains note
$BIN list | grep -q "^t1 *pink" && ok "list shows t1/pink" || bad "list shows t1/pink"

# 12. color change persists
$BIN color t1 blue >/dev/null
grep -q "color=blue" "$STICKIES_ROOT/notes/t1.meta" && ok "color persisted" || bad "color persisted"

# 13. move persists coords
$BIN move t1 42 24 >/dev/null
grep -q "x=42" "$STICKIES_ROOT/notes/t1.meta" && grep -q "y=24" "$STICKIES_ROOT/notes/t1.meta" \
  && ok "move persisted" || bad "move persisted"

# 14. edit replaces from stdin
echo "replaced" | $BIN edit t1 >/dev/null
check "edit replaces" "replaced" "$(cat "$STICKIES_ROOT/notes/t1.text" | tr -d '\n')"

# 15. edit on missing note fails
echo x | $BIN edit nosuch >/dev/null 2>&1 && bad "edit missing fails" || ok "edit missing fails"

# 16. rm deletes both files
$BIN rm t1 >/dev/null
[[ ! -e "$STICKIES_ROOT/notes/t1.text" && ! -e "$STICKIES_ROOT/notes/t1.meta" ]] \
  && ok "rm removed files" || bad "rm removed files"

# 17. rm missing fails
$BIN rm t1 >/dev/null 2>&1 && bad "rm missing fails" || ok "rm missing fails"

# 18. auto-name generation
for i in 1 2 3; do $BIN new -t "n$i" >/dev/null; done
names=$($BIN list | tail -n +2 | awk '{print $1}' | sort | tr '\n' ',')
check "auto names" "note1,note2,note3," "$names"

# 19. stdin body on new
echo "from stdin" | $BIN new sbody >/dev/null
check "stdin body" "from stdin" "$(cat "$STICKIES_ROOT/notes/sbody.text" | tr -d '\n')"

# 20. clear removes all
$BIN clear >/dev/null
check "cleared all" "(no stickies)" "$($BIN list)"

# 21. delete alias
$BIN new d1 -t x >/dev/null
$BIN delete d1 >/dev/null
[[ ! -e "$STICKIES_ROOT/notes/d1.text" ]] && ok "delete alias works" || bad "delete alias works"

# 22. unknown command exits 1
$BIN frob >/dev/null 2>&1; check "unknown cmd exit code" "1" "$?"

# 23. no args prints usage (exit 0)
$BIN | grep -q "Usage:" && ok "usage on no args" || bad "usage on no args"

rm -rf "$(dirname "$STICKIES_ROOT")"
echo "Stickies tests: $PASS passed, $FAIL failed"
[[ $FAIL -eq 0 ]]
