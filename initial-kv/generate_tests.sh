#!/usr/bin/env bash
set -euo pipefail

mkdir -p tests

# Helper function to generate test files
# Usage: create_test num "description" "run_command" rc "stdout" "stderr" [pre] [post]
create_test() {
  local num="$1"
  local desc="$2"
  local run_cmd="$3"
  local rc="$4"
  local expected_out="$5"
  local expected_err="$6"
  local pre="${7:-}"
  local post="${8:-}"

  echo "$desc" >"tests/${num}.desc"
  echo "$run_cmd" >"tests/${num}.run"
  echo "$rc" >"tests/${num}.rc"
  printf "%s" "$expected_out" >"tests/${num}.out"
  printf "%s" "$expected_err" >"tests/${num}.err"

  [[ -n "$pre" ]] && echo "$pre" >"tests/${num}.pre" || rm -f "tests/${num}.pre"
  [[ -n "$post" ]] && echo "$post" >"tests/${num}.post" || rm -f "tests/${num}.post"
}

# One-time build setup
cat <<'EOF' >tests/pre
gcc -Wall -Wextra -Werror -O2 -o kv kv.c
EOF

# -----------------------------------------------------------------------------
# Test 1: Empty run / Immediate EOF
# -----------------------------------------------------------------------------
create_test 1 "Empty input: prints single prompt and exits" \
  "./kv < /dev/null" \
  0 \
  "> " \
  "" \
  "rm -f database.txt" \
  "rm -f database.txt"

# -----------------------------------------------------------------------------
# Test 2: Single PUT and GET in one session
# -----------------------------------------------------------------------------
create_test 2 "Basic PUT and GET in single session" \
  "printf 'p,10,alpha\ng,10\n' | ./kv" \
  0 \
  "> > 10,alpha
> " \
  "" \
  "rm -f database.txt" \
  "rm -f database.txt"

# -----------------------------------------------------------------------------
# Test 3: Multiple commands on a single line
# -----------------------------------------------------------------------------
create_test 3 "Multiple space-separated commands on one line" \
  "printf 'p,1,first p,2,second g,1 g,2\n' | ./kv" \
  0 \
  "> 1,first
2,second
> " \
  "" \
  "rm -f database.txt" \
  "rm -f database.txt"

# -----------------------------------------------------------------------------
# Test 4: Key not found on GET
# -----------------------------------------------------------------------------
create_test 4 "GET nonexistent key reports not found" \
  "printf 'g,999\n' | ./kv" \
  0 \
  "> > " \
  "999 not found
" \
  "rm -f database.txt" \
  "rm -f database.txt"

# -----------------------------------------------------------------------------
# Test 5: DELETE existing key
# -----------------------------------------------------------------------------
create_test 5 "DELETE removes key and subsequent GET fails" \
  "printf 'p,42,answer\nd,42\ng,42\n' | ./kv" \
  0 \
  "> > > > " \
  "42 not found
" \
  "rm -f database.txt" \
  "rm -f database.txt"

# -----------------------------------------------------------------------------
# Test 6: DELETE non-existent key
# -----------------------------------------------------------------------------
create_test 6 "DELETE nonexistent key reports not found" \
  "printf 'd,55\n' | ./kv" \
  0 \
  "> > " \
  "55 not found
" \
  "rm -f database.txt" \
  "rm -f database.txt"

# -----------------------------------------------------------------------------
# Test 7: Bad command handling without crashing
# -----------------------------------------------------------------------------
create_test 7 "Invalid command emits error and continues processing" \
  "printf 'x,1,foo p,100,valid g,100\n' | ./kv" \
  0 \
  "> 100,valid
> " \
  "bad command
" \
  "rm -f database.txt" \
  "rm -f database.txt"

# -----------------------------------------------------------------------------
# Test 8: Value update / Overwrite
# -----------------------------------------------------------------------------
create_test 8 "PUT with identical key updates existing value" \
  "printf 'p,5,initial\np,5,updated\ng,5\n' | ./kv" \
  0 \
  "> > > 5,updated
> " \
  "" \
  "rm -f database.txt" \
  "rm -f database.txt"

# -----------------------------------------------------------------------------
# Test 9: ALL command (sorted order)
# -----------------------------------------------------------------------------
create_test 9 "ALL command dumps entries sorted by key" \
  "printf 'p,30,c p,10,a p,20,b\na\n' | ./kv" \
  0 \
  "> > 10,a
20,b
30,c
> " \
  "" \
  "rm -f database.txt" \
  "rm -f database.txt"

# -----------------------------------------------------------------------------
# Test 10: CLEAR command
# -----------------------------------------------------------------------------
create_test 10 "CLEAR command empties entire vector" \
  "printf 'p,1,x p,2,y\nc\na\ng,1\n' | ./kv" \
  0 \
  "> > > > > " \
  "1 not found
" \
  "rm -f database.txt" \
  "rm -f database.txt"

# -----------------------------------------------------------------------------
# Test 11: Persistence across separate REPL invocations
# -----------------------------------------------------------------------------
create_test 11 "Persistence across multiple separate process invocations" \
  "printf 'p,101,persisted_val\n' | ./kv > /dev/null && printf 'g,101\n' | ./kv" \
  0 \
  "> 101,persisted_val
> " \
  "" \
  "rm -f database.txt" \
  "rm -f database.txt"

# -----------------------------------------------------------------------------
# Test 12: Persistence with Deletion
# -----------------------------------------------------------------------------
create_test 12 "Persistence preserves deleted state across runs" \
  "printf 'p,1,a p,2,b\n' | ./kv > /dev/null && printf 'd,1\n' | ./kv > /dev/null && printf 'a\n' | ./kv" \
  0 \
  "> 2,b
> " \
  "" \
  "rm -f database.txt" \
  "rm -f database.txt"

# -----------------------------------------------------------------------------
# Test 13: Empty lines and whitespace tolerance
# -----------------------------------------------------------------------------
create_test 13 "Gracefully ignore blank lines" \
  "printf '\n  \np,1,val\n\ng,1\n' | ./kv" \
  0 \
  "> > > > > 1,val
> " \
  "" \
  "rm -f database.txt" \
  "rm -f database.txt"

# -----------------------------------------------------------------------------
# Test 14: Capacity growth / Dynamic realloc stress test (500 items)
# -----------------------------------------------------------------------------
# Generate 500 inserts and a query for item #250
PRE_14='
rm -f database.txt input_14.txt
for i in $(seq 1 500); do
  echo "p,$i,val$i" >> input_14.txt
done
echo "g,250" >> input_14.txt
'
create_test 14 "Realloc stress test exceeding default 128-element buffer" \
  "./kv < input_14.txt" \
  0 \
  "$(
    for i in $(seq 1 500); do printf "> "; done
    printf "> 250,val250\n> "
  )" \
  "" \
  "$PRE_14" \
  "rm -f input_14.txt database.txt"

# -----------------------------------------------------------------------------
# Test 15: Reverse-order insertion stress test (forces memmove shifts)
# -----------------------------------------------------------------------------
PRE_15='
rm -f database.txt input_15.txt
for i in $(seq 300 -1 1); do
  echo "p,$i,v$i" >> input_15.txt
done
echo "g,1" >> input_15.txt
echo "g,300" >> input_15.txt
'
create_test 15 "Reverse-ordered insertions stressing vector shifting logic" \
  "./kv < input_15.txt" \
  0 \
  "$(
    for i in $(seq 1 300); do printf "> "; done
    printf "> 1,v1\n> 300,v300\n> "
  )" \
  "" \
  "$PRE_15" \
  "rm -f input_15.txt database.txt"

# -----------------------------------------------------------------------------
# Test 16: Interleaved mixed operations under load
# -----------------------------------------------------------------------------
PRE_16='
rm -f database.txt input_16.txt
echo "p,10,a p,20,b p,30,c" >> input_16.txt
echo "d,20 badcmd p,25,mid" >> input_16.txt
echo "a" >> input_16.txt
'
create_test 16 "Interleaved commands on shared lines with errors and mutations" \
  "./kv < input_16.txt" \
  0 \
  "> > > 10,a
25,mid
30,c
> " \
  "bad command
" \
  "$PRE_16" \
  "rm -f input_16.txt database.txt"

echo "Generated 16 tests successfully in tests/"
