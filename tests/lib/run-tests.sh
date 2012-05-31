#!/bin/bash

export LANG=C

GREEN="$(echo -e '\033[1;32m')"
YELLOW="$(echo -e '\033[0;33m')"
RED="$(echo -e '\033[1;31m')"
NORMAL="$(echo -e '\033[0;39m')"

CFLAGS="$CFLAGS -Wl,-rpath,\$ORIGIN/../../src -I ../../src/include/public"
LDFLAGS="$LDFLAGS ../../src/libmonkey.so*"
[ -z "$CC" ] && CC=gcc

success=0
fail=0

for src in *.c; do
	test=${src%.c}

	echo -n "Building test $test... "
	$CC $CFLAGS $LDFLAGS -o $test $src
	if [ $? -ne 0 ]; then
		fail=$((fail + 1))
		continue
	fi

	./$test
	if [ $? -ne 0 ]; then
		fail=$((fail + 1))
		echo "${RED}Failed $NORMAL"
	else
		success=$((success + 1))
		echo
	fi
done

echo

total=$((fail + success))
percentage=$(awk "BEGIN{print $success/$total * 100}")
percentage=$(printf '%.2f' $percentage)

num=${percentage//.*/}

[ $fail -eq 0 ] && echo "$GREEN	All tests passed!"
[ "$num" -le 90 -a "$num" -ge 60 ] && echo "$YELLOW	$percentage% passed, $fail fails"
[ $num -lt 60 ] && echo "$RED	$percentage% passed, $fail fails"

echo $NORMAL
