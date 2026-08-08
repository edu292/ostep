#!/usr/bin/env bash
# test-pzip.sh

WZIP="../initial-utilities/wzip/wzip"

echo "=> Generating 15 test cases using reference wzip..."
mkdir -p tests

# Helper function to generate test case scaffolding
gen_test() {
    local n=$1
    local desc=$2
    local args=$3
    local rc=$4
    local err=$5
    printf "%s\n" "$rc" >tests/"$n".rc
    printf "%b" "$err" >tests/"$n".err
    printf "./pzip %s\n" "$args" >tests/"$n".run
    printf "%s\n" "$desc" >tests/"$n".desc
}

# T1: No args
gen_test 1 "No arguments" "" 1 "pzip: file1 [file2 ...]\n"
touch tests/1.out

# T2: Bad file
gen_test 2 "Bad file" "does_not_exist.txt" 1 "pzip: cannot open file\n"
touch tests/2.out

# T3: Empty file
touch tests/3.in
touch tests/3.out
gen_test 3 "Empty file" "tests/3.in" 0 ""

# T4: Single char
echo "a" >tests/4.in
$WZIP tests/4.in >tests/4.out
gen_test 4 "Single char" "tests/4.in" 0 ""

# T5: Repeating chars
echo "aaaaaaaaaa" >tests/5.in
$WZIP tests/5.in >tests/5.out
gen_test 5 "Repeating chars" "tests/5.in" 0 ""

# T6: Distinct chars
echo "aaabbbb" >tests/6.in
$WZIP tests/6.in >tests/6.out
gen_test 6 "Distinct chars" "tests/6.in" 0 ""

# T7: Alternating chars
echo "abababab" >tests/7.in
$WZIP tests/7.in >tests/7.out
gen_test 7 "Alternating chars" "tests/7.in" 0 ""

# T8: Newlines and spaces
printf "a \n \n b" >tests/8.in
$WZIP tests/8.in >tests/8.out
gen_test 8 "Newlines and spaces" "tests/8.in" 0 ""

# T9: Numbers and symbols
echo "111@@@222!!!" >tests/9.in
$WZIP tests/9.in >tests/9.out
gen_test 9 "Numbers and symbols" "tests/9.in" 0 ""

# T10: Two files, distinct boundary
echo "aaa" >tests/10a.in
echo "bbb" >tests/10b.in
$WZIP tests/10a.in tests/10b.in >tests/10.out
gen_test 10 "Two files, distinct boundary" "tests/10a.in tests/10b.in" 0 ""

# T11: Two files, merging boundary (CRITICAL FOR PARALLEL PZIP)
echo "aaa" >tests/11a.in
echo "aaa" >tests/11b.in
$WZIP tests/11a.in tests/11b.in >tests/11.out
gen_test 11 "Two files, merging boundary" "tests/11a.in tests/11b.in" 0 ""

# T12: Three files, cascading merge
echo "a" >tests/12a.in
echo "a" >tests/12b.in
echo "a" >tests/12c.in
$WZIP tests/12a.in tests/12b.in tests/12c.in >tests/12.out
gen_test 12 "Three files, cascading merge" "tests/12a.in tests/12b.in tests/12c.in" 0 ""

# T13: Large single file (10,000 chars)
yes "z" | head -n 10000 | tr -d '\n' >tests/13.in
$WZIP tests/13.in >tests/13.out
gen_test 13 "Large single file" "tests/13.in" 0 ""

# T14: Mixed empty and non-empty files
echo "a" >tests/14a.in
touch tests/14b.in
echo "a" >tests/14c.in
$WZIP tests/14a.in tests/14b.in tests/14c.in >tests/14.out
gen_test 14 "Mixed empty and non-empty files" "tests/14a.in tests/14b.in tests/14c.in" 0 ""

# T15: Many files
touch tests/15a.in tests/15b.in tests/15c.in
echo "x" >tests/15a.in
echo "y" >tests/15b.in
echo "z" >tests/15c.in
$WZIP tests/15a.in tests/15b.in tests/15c.in >tests/15.out
gen_test 15 "Many files" "tests/15a.in tests/15b.in tests/15c.in" 0 ""
