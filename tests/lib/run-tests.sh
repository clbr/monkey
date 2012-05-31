#!/bin/bash

export LANG=C

GREEN="$(echo -e '\033[1;32m')"
YELLOW="$(echo -e '\033[0;33m')"
RED="$(echo -e '\033[1;31m')"
NORMAL="$(echo -e '\033[0;39m')"

CFLAGS="$CFLAGS -Wl,-rpath,\$ORIGIN/../../src -I ../../src/include/public"
LDFLAGS="$LDFLAGS ../../src/libmonkey.so*"
[ -z "$CC" ] && CC=gcc

# Precompile the header for faster builds
$CC ../../src/include/public/monkey.h

success=0
fail=0

for src in *.c; do
	[ ! -f "$src" ] && exit

	test=${src%.c}
	log=${test}.log

	echo -n "Building test $test... "
	$CC $CFLAGS $LDFLAGS -o $test $src
	if [ $? -ne 0 ]; then
		fail=$((fail + 1))
		echo "${RED}Failed to build $NORMAL"
		continue
	fi

	./$test > $log
	if [ $? -ne 0 ]; then
		fail=$((fail + 1))
		echo "${RED}Failed $NORMAL"
	else
		success=$((success + 1))
		echo
		rm -f $log
	fi

	# If empty, remove
	[ ! -s "$log" ] && rm -f $log
done

# Remove the PCH
rm ../../src/include/public/monkey.h.gch


echo

total=$((fail + success))
percentage=$(awk "BEGIN{print $success/$total * 100}")
percentage=$(printf '%.2f' $percentage)

num=${percentage//.*/}

[ $fail -eq 0 ] && echo "$GREEN	All tests passed!"
[ "$num" -le 90 -a "$num" -ge 60 ] && echo "$YELLOW	$percentage% passed, $fail/$total fails"
[ $num -lt 60 ] && echo "$RED	$percentage% passed, $fail/$total fails"

echo $NORMAL
