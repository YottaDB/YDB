#!/usr/bin/env bash

#################################################################
#								#
# Copyright (c) 2020 YottaDB LLC and/or its subsidiaries.	#
# All rights reserved.						#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

# Exit on error; disallow unset variables; print each command as it is executed
set -euv
# If any command in a pipeline fails, count the entire pipeline as failed
set -o pipefail
# `sort` has different output depending on the locale
export LC_ALL=C

# TODO(#644): occasionally run clang-tidy in release mode
build_type="Debug"
echo " -> build_type = $build_type"

echo "# Run the build using clang"
mkdir build
cd build
cmake -D CMAKE_C_COMPILER=clang-10 -D CMAKE_BUILD_TYPE=$build_type -D CMAKE_EXPORT_COMPILE_COMMANDS=ON ..
mkdir warnings
# Record the warnings, but if `make` fails, say why instead of silently exiting.
set +e
# Since we're only looking for warnings, we don't actually need to do a full build.
# Only generate .c and .h files that clang-tidy will need.
# NOTE: `clang-tidy` will give all the same warnings `clang` would have, so we're not ignoring anything.
make -j $(nproc) gen_export check_git_repository 2>warnings/make_warnings.txt
status=$?
set -e
cd warnings
if ! [ $status = 0 ]; then
	echo "# make failed with exit status [$status]. make output follows below"
	cat make_warnings.txt
	exit $status
fi

# Check for unexpected warnings
../../ci/sort_warnings.sh make_warnings.txt

# This is used for both `make` and `clang-tidy` warnings.
compare() {
	expected="$1"
	actual="$2"
	original_warnings="$3"
	if [ -e "$expected" ]; then
		# We do not want any failures in "diff" command below to exit the script (we want to see the actual diff a few steps later).
		# So never count this step as failing even if the output does not match.
		# NOTE: ignores trailing spaces, because clang-tidy outputs extra spaces but `sort_warnings.sh` strips them.
		diff -Z "$expected" "$actual" &> differences.txt || true

		if [ $(wc -l differences.txt | awk '{print $1}') -gt 0 ]; then
			echo " -> Expected warnings differ from actual warnings! diff output follows"
			echo " -> note: '<' indicates an expected warning, '>' indicates an actual warning"
			cat differences.txt
			echo
			grep -Ev '^(Database file |%YDB-I-DBFILECREATED, |%GDE-I-|YDB-MUMPS\[)' $original_warnings \
				| grep -v '\.gld$' > $original_warnings.stripped || true;
			echo " -> note: the original warnings are available in $original_warnings.stripped"
			echo " -> note: the expected warnings are available in $expected"
			exit 1
		fi
	else
		# Make sure that there are actually no warnings, not just that someone forgot to add a .ref file
		[ $(wc -c < "$actual") = 0 ]
	fi
}
compare ../../ci/warnings.ref sorted_warnings.txt make_warnings.txt

cd ..
../ci/create_tidy_warnings.sh warnings .

# NOTE: If this command fails, you can download `sorted_warnings.txt` to `ci/tidy_warnings.ref`
compare ../ci/tidy_warnings.ref warnings/{sorted_warnings.txt,tidy_warnings.txt}
