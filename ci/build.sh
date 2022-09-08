#!/usr/bin/env bash

#################################################################
#								#
# Copyright (c) 2020-2022 YottaDB LLC and/or its subsidiaries.	#
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

echo "# Randomly choose to build Debug or Release build"
if [[ $(( RANDOM % 2)) -eq 0 ]]; then
	build_type="Debug"
else
	build_type="RelWithDebInfo"
fi
echo " -> build_type = $build_type"

echo "# Run shellcheck on all scripts"
if [ -x "$(command -v shellcheck)" ]; then
	find . -name '*.sh' -print0 | xargs -0 shellcheck -e SC1091,SC2154,SC1090,SC2086,SC2053,SC2046,SC2006,SC2164
else
	echo " -> Shellcheck not found!"
	exit 1
fi

echo "# Check fuzzing/*.patch list of files matches those in fuzzing/Makefile"
(
cd fuzzing
set +e	# Needed to record the failures with error detail, instead of silently exiting.
exit_status=0
for file in *.patch
do
	grep 'patch --no-backup-if-mismatch' Makefile | grep $file
	status=$?
	if ! [ $status = 0 ]; then
		echo "FAILED : [$file] needs to be added to the list of files in [patch] target section of [fuzzing/Makefile]"
		exit_status=1
	fi
	grep 'patch --reject-file=-' Makefile | grep $file
	status=$?
	if ! [ $status = 0 ]; then
		echo "FAILED : [$file] needs to be added to the list of files in [reset] target section of [fuzzing/Makefile]"
		exit_status=1
	fi
done
exit $exit_status
)

echo "# Run the build using clang"
rm -rf build # if it already exists
mkdir build
cd build || exit
cmake -D CMAKE_C_COMPILER=clang-14 -D CMAKE_BUILD_TYPE=$build_type -D CMAKE_EXPORT_COMPILE_COMMANDS=ON ..
mkdir warnings
# Record the warnings, but if `make` fails, say why instead of silently exiting.
set +e
# Since we're only looking for warnings, we don't actually need to do a full build.
# Only generate .c and .h files that clang-tidy will need.
# NOTE: `clang-tidy` will give all the same warnings `clang` would have, so we're not ignoring anything.
make -j $(nproc) gen_export check_git_repository 2>warnings/make_warnings.txt
status=$?
set -e
cd warnings || exit
if ! [ $status = 0 ]; then
	echo "# make failed with exit status [$status]. make output follows below"
	cat make_warnings.txt
	exit $status
fi

# Check for unexpected warnings
../../ci/sort_warnings.sh make_warnings.txt sorted_warnings.txt

# This is used for both `make` and `clang-tidy` warnings.
# It should be run from the warnings/ directory.
compare() {
	expected="$1"
	actual="$2"
	original_warnings="$3"
	if [ -e "$expected" ]; then
		# We do not want any failures in "diff" command below to exit the script (we want to see the actual diff a few steps later).
		# So never count this step as failing even if the output does not match.
		# NOTE: ignores trailing spaces, because clang-tidy outputs extra spaces but `sort_warnings.sh` strips them.
		echo "# Running command : diff -Z $expected $actual >& differences.txt"
		diff -Z "$expected" "$actual" &> differences.txt || true

		if [ $(wc -l differences.txt | awk '{print $1}') -gt 0 ]; then
			{
				echo " -> Expected warnings differ from actual warnings! diff output follows"
				echo " -> note: '<' indicates an expected warning, '>' indicates an actual warning"
				cat differences.txt
				echo
				grep -Ev '^(Database file |%YDB-I-DBFILECREATED, |%GDE-I-|YDB-MUMPS\[)' $original_warnings \
					| grep -v '\.gld$' > $original_warnings.stripped || true;
				echo " -> note: the original warnings are available in $original_warnings.stripped"
				echo " -> note: the expected warnings are available in $expected"
				if [ $build_type = Debug ]; then
					other_job=RelWithDebInfo
				else
					other_job=Debug
				fi
				echo " -> warning: since the $build_type job failed, the $other_job job will likely also fail. Make sure you update the relevant warnings for both."
			} > warnings/summary.txt
			cat warnings/summary.txt
			exit 1
		fi
	else
		# Make sure that there are actually no warnings, not just that someone forgot to add a .ref file
		[ $(wc -c < "$actual") = 0 ]
	fi
}

if [ $build_type = Debug ]; then
	compare ../../ci/warnings.ref sorted_warnings.txt make_warnings.txt
else
	compare ../../ci/warnings_rel.ref sorted_warnings.txt make_warnings.txt
fi

cd ..
../ci/create_tidy_warnings.sh warnings .

# NOTE: If this command fails, you can download `sorted_warnings.txt` to `ci/tidy_warnings_debug.ref`
if [ $build_type = Debug ]; then
	compare ../ci/tidy_warnings_debug.ref warnings/{sorted_warnings.txt,tidy_warnings.txt}
else
	compare ../ci/tidy_warnings_release.ref warnings/{sorted_warnings.txt,tidy_warnings.txt}
fi
