#!/usr/bin/env bash

# Exit on error; disallow unset variables; print each command as it is executed
set -euv
# If any command in a pipeline fails, count the entire pipeline as failed
set -o pipefail

echo "# Randomly choose to build Debug or Release build"
if [[ $(( $RANDOM % 2)) -eq 0 ]]; then
	build_type="Debug"
else
	build_type="RelWithDebInfo"
fi
echo " -> build_type = $build_type"

echo "# Run the build using clang"
mkdir build
cd build
cmake -D CMAKE_C_COMPILER=clang-10 -D CMAKE_BUILD_TYPE=$build_type -D CMAKE_EXPORT_COMPILE_COMMANDS=ON ..
# Record the warnings, but if `make` fails, say why instead of silently existing.
set +e
make -j $(nproc) 2>make_warnings.txt
status=$?
set -e
if ! [ $status = 0 ]; then
	echo "# make failed with exit status [$status]. make output follows below"
	cat make_warnings.txt
	exit $status
fi

# Check for unexpected warnings
../ci/sort_warnings.sh make_warnings.txt

# This is used for both `make` and `clang-tidy` warnings.
compare() {
	expected="$1"
	actual="$2"
	warnings="$3"
	if [ -e "$expected" ]; then
		# We do not want any failures in "diff" command below to exit the script (we want to see the actual diff a few steps later).
		# So never count this step as failing even if the output does not match.
		diff "$expected" sorted_warnings.txt &> differences.txt || true

		if [ $(wc -l differences.txt | awk '{print $1}') -gt 0 ]; then
			echo " -> Expected warnings differ from actual warnings! diff output follows"
			echo " -> note: '<' indicates an expected warning, '>' indicates an actual warning"
			cat differences.txt
			echo
			echo " -> note: these were the original warnings:"
			grep -Ev '^(Database file |%YDB-I-DBFILECREATED, |%GDE-I-|YDB-MUMPS\[)' $warnings \
				| grep -v '\.gld$'
			echo
			echo " -> note: the expected warnings are available in $expected"
			exit 1
		fi
	else
		# Make sure that there are actually no warnings, not just that someone forgot to add a .ref file
		[ $(wc -c < "$actual") = 0 ]
	fi
}
compare ../ci/warnings.ref sorted_warnings.txt make_warnings.txt

# Run clang-tidy, but only on C files that have been modified.
# This avoids ballooning the pipeline times to nearly an hour.
# NOTE: this does *not* run on header files, because YDB style is not to
# include headers in other headers when they were included in the original .c
# file. So trying to run `clang-tidy` on them gives lots of errors about
# unknown typedefs.
# NOTE: adds `../` to start of files, since git shows them relative to the top-level directory.
git diff --name-only HEAD~ \
	| { grep -E 'sr_(linux|unix|port|x86_64)/.*\.c$' || true; } \
	| sed 's#^#../#' \
	>modified_files.txt

# NOTE: doesn't run `insecureAPI` because it has many thousands of false positives
# In particular, it warns any time you use strcpy.
xargs < modified_files.txt --no-run-if-empty \
	parallel clang-tidy-10 '--checks=-clang-analyzer-security.insecureAPI.*' -- \
	>tidy_warnings.txt

../ci/sort_warnings.sh tidy_warnings.txt
# Strip `../<dir>/` from file names
sed 's#^\.\./[^/]*/##' modified_files.txt > modified_files_stripped.txt
# Only compare warnings from modified files
# Don't exit with an error if there were no files modified.
grep --fixed-strings --file modified_files_stripped.txt ../ci/tidy_warnings.ref > expected_warnings.ref || true
compare expected_warnings.ref sorted_warnings.txt tidy_warnings.txt
