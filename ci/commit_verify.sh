#!/usr/bin/env bash
#################################################################
#								#
# Copyright (c) 2020-2026 YottaDB LLC and/or its subsidiaries.	#
# All rights reserved.						#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

set -euv
set -o pipefail

if [ $# -lt 1 ] || [ -z "$1" ]; then
	echo "usage: $0 <needs_copyright.sh> <remote URL> [comparison branch]"
	exit 1
fi
if ! [ -r "$1" ]; then
	echo "error: $1 must exist and be readable"
	exit 1
fi

needs_copyright="$1"
upstream_repo="$2"
target_branch="${3:-master}"

echo "# Check for a commit that was not gpg-signed"

# If you need to add a new key ID, add it to the below list.
# The key will also have to be on a public key server.
# If you have never published it before, you can do so with
# `gpg --keyserver hkps://keyserver.ubuntu.com --send-keys KEY_ID`.
# You can find your key ID with `gpg -K`;
# upload all keys that you will use to sign commits.
GPG_KEYS=(
    # Jynn Nelson
    "A7AC371F484CB10A0EC1CAE1964534138B7FB42E"
    "A833B01B44BDDA1EE80E261CE655823D8D8B1088"
    "A463388F6BD7260BDB9C043D9C545D04A0181688"
    # Jon Badiali
    "58FE3050D9F48C46FC506C68C5C2A5A682CB4DDF"
    "2B10483F17B011CE749F77B50F7AF042F822E202"
    # Brad Westhafer
    "FC859B0401A6C5F92BF1C1061E46090B0FD0A34E"
    # Jaahanavee Sikri
    "192D7DD0968178F057B2105DE5E6FCCE1994F0DD"
    # Narayanan Iyer
    "0CE9E4C2C6E642FE14C1BB9B3892ED765A912982"
    # Ganesh Mahesh
    "74774D6D0DB17665AB75A18211A87E9521F6FB86"
    # K S Bhaskar
    "D3CFECF187AECEAA67054719DCF03D8B30F73415"
    # Steven Estes
    "CC9A13F429C7F9231E4DFB2832655C57E597CF83"
    # David Wicksell
    "9647BC597DF8423D0A439C70CD56BF371555A00E"
    "74B3BE040ED458D6F32AADE46321D94F6FD1C8BB"
    # Srijan Pandey
    "7C9A96B7539C80903AE5FEB1F963F9C6E941578F"
    # Keziah Zapanta
    "729F7108E49E9AAF293EC5F06A06ABEB0DD4B446"
    # Peter Goss
    "583cdd3db91045a5eddda9b58ab10d34126a4839"
    # Sam Habiel
    "48A12817C70BEB6047CB45365BD24AD20D6FDEDD"
    # Ashok Bhaskar
    "71D2337F30219A12153AB2634529231C7717E22C"
    # Ahmed Abdelrazek
    "415769A05CF58DC69E01CBC4FDC033E3C88BEEBE"
    # Thammanoon Phattramarut
    "34BEF76BD1AC8C543F6C0B4C377169AEEA0EA8CD"
    # Konstantin Aristov
    "597B70286C90AF5A47B03AEB86616B621FF177CC"
    # Stefano Lalli
    "7DFE4A199F6CEF4930E8116F9C97353925E898E9"
    # Berwyn Hoyt
    "0135A87314F8DE1D16C66011A02724281164EA4F"
    # Tomas Morstein
    "A8C4D3290201B5647F557BC8135280C2690FCF0F"
    # Erno Zalka
    "67D89209E54981F1023D915DBDFD816F09C10D01"
    # Comsan Chanma
    "D2590D2F68BC2620370D52EBDEEEA9DC8B9A55D8"
    # Jirapat Distaprom
    "146843872A57D45E8CB06841295AC8BD2134D07E"
    # Serena Willis
    "8C3416F3D6FD7624914704A22B379ED58A1511C0"
    # Ben Chermside
    "F0EE5AF195B29FBBBE53A2099B40A867535E7187"
)
gpg --keyserver hkps://keyserver.ubuntu.com --recv-keys "${GPG_KEYS[@]}"

echo "# Add $upstream_repo as remote"
git remote -v
if ! git remote | grep -q upstream_repo; then
	git remote add upstream_repo "$upstream_repo"
fi
git fetch upstream_repo

echo "target/upstream branch set to: $target_branch"

echo "# Fetch all commit ids only present in MR by comparing to target/upstream $target_branch branch"
COMMIT_IDS=`git rev-list upstream_repo/$target_branch..HEAD`

if [ -z "$COMMIT_IDS" ]; then
	# This is not the normal case. Only occurs when MR is merged and pipeline happens in the master branch on upstream repo.
	# In this case, do commit verification on the latest 1 commit (i.e. HEAD).
	COMMIT_IDS=`git rev-list HEAD~1..HEAD`
	# In this case, it is possible that when the master branch pipeline runs the date has moved forward by 1 year
	# relative to when the latest 1 commit got merged. In that case, the copyright check should use the year of the
	# commit instead of the current year. Hence the below code to retrieve that year from the latest commit.
	# If the commit contains some newly created files, there will be additional lines output of the form "create mode 100644"
	# for each newly created file. We do not want that. Hence filter that out using "sed -n 1p". Do not use "head -1" as
	# it can cause SIGPIPE errors on the "git show" side (exit status 141) in case "git show" has a lot of output since
	# we also have "set -o pipefail" set. See https://unix.stackexchange.com/a/738538 for the "sed" suggestion.
	curyear=$(git show --summary --pretty=format:'%cd' --date=format:%Y HEAD | sed -n 1p)
else
	# This is the normal case when the pipeline runs as part of an open MR in origin repo.
	# Use the current year for copyright checks.
	curyear="$(date +%Y)"
fi

echo "${COMMIT_IDS[@]}"

echo "# Verify commits and copyrights"

commit_list=""
for id in $COMMIT_IDS
do
	if ! git verify-commit "$id" > /dev/null 2>&1; then
		echo "  --> Error: commit $id was not signed with a known GPG key!"
		exit 1
	fi
	# Get commit message from id
	COMMIT_MESSAGE=`git log --format=%B -n 1 $id`
	# 1) Skip the copyright check if the commit message contains
	#    "Merge GT.M Vx.x-xxx into YottaDB mainline (with conflicts)"
	# 2) Check for any "git revert" commits. Those have the case-sensitive string 'Revert ' somewhere in the commit title.
	#    Those can undo changes and so can result in a file having a copyright notice that is not the current year.
	#    Therfore skip the copyright check in those commits too.
	if [[ "$COMMIT_MESSAGE" =~ .*Merge[[:space:]]GT\.M[[:space:]]V[0-9]\.[0-9]-[0-9]{3}[[:space:]]into[[:space:]]YottaDB[[:space:]]mainline[[:space:]]\(with[[:space:]]conflicts\).* ]]; then
		echo "Skipping copyright check for commit : $id as it contains [Merge GT.M Vx.x-xxx into YottaDB mainline (with conflicts)]"
	elif [[ "$COMMIT_MESSAGE" =~ "Revert " ]]; then
		echo "Skipping copyright check for commit : $id as it contains [Revert ] in the title"
	else
		commit_list="$commit_list $id"
	fi
done

# First check if there is a non-zero list of commits to verify. If the commit list is empty, skip this verification step.
exit_status=0
if [ -n "$commit_list" ]; then
	# Get file list from all commits at once, rather than per-commit
	filelist="$(git show --pretty="" --name-only $commit_list | sort -u)"
	missing_files=""
	copyright_only=""
	for file in $filelist; do
		if [[ ! -e $file ]]; then
			continue
		fi
		# Get number of changes (addition deletion file_name)
		changes=$(git diff --numstat upstream_repo/$target_branch..HEAD "$file")
		if [[ -n "$changes" ]]; then
			additions=$(echo $changes | awk '{print $1}')
			deletions=$(echo $changes | awk '{print $2}')
			if [[ (( $additions == 0 )) && (( $deletions == 0 )) ]]; then
				continue
			fi
			# Search not only for Copyright lines but also for surrounding context when counting number of lines
			# that are added/deleted for Copyright. Or else we could miss out on signaling copyright-only changes
			# like is described at https://gitlab.com/YottaDB/DB/YDB/-/merge_requests/1323#note_1599267130.
			#
			# Note that git diff output would include 4 lines starting which include lines of the form "+++ " or "--- "
			# which would confuse the count when searching for lines starting with "^+" and "^-" in the diff output.
			# In the most common case, the git diff output would be something like the following.
			#
			#	diff --git a/sr_unix/setactive.csh b/sr_unix/setactive.csh
			#	index 41ce59aa..83d2f7b2 100755
			#	--- a/sr_unix/setactive.csh
			#	+++ b/sr_unix/setactive.csh
			#	@@ -3,6 +3,9 @@
			#
			# Therefore using "tail -n +5" to filter out the first 4 lines of the git diff output would
			# let us start printing lines from the "@@ -3,6..." line onwards.
			#
			# But git diff output could include 5 or 6 lines in some edge cases.
			#
			# a) In case a file got newly created, it would have 1 more line to indicate the mode of the new file.
			#
			#      Example output below.
			#
			#	diff --git a/sr_unix/x.csh b/sr_unix/x.csh
			#	new file mode 100755
			#	index 00000000..83d2f7b2
			#	--- /dev/null
			#	+++ b/sr_unix/x.csh
			#	@@ -0,0 +1,154 @@
			#
			#    In this case, we should do a "tail -n +5" to get to the "@@ -0,0..." line.
			#
			# b) In case a file's mode also got changed, it would have 2 more lines to indicate the old and new modes.
			#
			#      Example output below.
			#
			#	diff --git a/sr_unix/setactive.csh b/sr_unix/setactive.csh
			#	old mode 100755
			#	new mode 100644
			#	index 41ce59aa..83d2f7b2
			#	--- a/sr_unix/setactive.csh
			#	+++ b/sr_unix/setactive.csh
			#	@@ -3,6 +3,9 @@
			#
			#    In this case, we should do a "tail -n +6" to get to the "@@ -0,0..." line.
			#
			# Since our final goal is to reach the line starting with "@@ " which is there in all the above cases,
			# we use a "sed" command to filter out all lines in the git diff output until we see a "^@@ ".
			#
			# Also note that "grep -C" will by default include a group separator line ("--") to demarcate contiguous
			# groups of matches. But that would again be confused as deleted lines in the git diff output so we disable
			# that by adding a "--no-group-separator" to the "grep -C".
			#
			num_copyright_additions=$(git diff upstream_repo/$target_branch..HEAD "$file" | sed -n '/^@@ /,$p' | grep --no-group-separator -C 3 'Copyright (c)' | grep -c -e '^+' || true)
			num_copyright_deletions=$(git diff upstream_repo/$target_branch..HEAD "$file" | sed -n '/^@@ /,$p' |  grep --no-group-separator -C 3 'Copyright (c)' | grep -c -e '^-' || true)
			if [[ (( $deletions == $num_copyright_deletions )) && (( $additions == $num_copyright_additions )) ]]; then
				if [[ (( "ci/commit_verify.sh" != $file )) && (( "sr_port/copyright.txt" != $file )) ]]; then
					# Avoid copyright-only check on this framework script
					copyright_only="$copyright_only $file"
				fi
			fi
			if $needs_copyright $file && ! grep -q 'Copyright (c) .*'$curyear' YottaDB LLC' $file; then
				# Print these out only at the end so they're all shown at once
				missing_files="$missing_files $file"
			fi
		fi
	done
	if [ -n "$copyright_only" ]; then
		echo "  --> Error: some files contain only copyright changes"
		for file in $copyright_only; do
			echo "	$file"
		done
		exit_status=1
	fi
	if [ -n "$missing_files" ]; then
		echo "  --> Error: some files are missing a YottaDB Copyright notice and/or current year $curyear"
		for file in $missing_files; do
			echo "	$file"
		done
		exit_status=1
	fi
fi
exit $exit_status
