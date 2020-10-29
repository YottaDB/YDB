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

set -euv
set -o pipefail

if [ $# -lt 1 ] || [ -z "$1" ]; then
	echo "usage: $0 <needs_copyright.sh> <remote URL> [comparison branch]"
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
    # Joshua Nelson
    "A7AC371F484CB10A0EC1CAE1964534138B7FB42E"
    # Jon Badiali
    "58FE3050D9F48C46FC506C68C5C2A5A682CB4DDF"
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
    # Steven Estess
    "CC9A13F429C7F9231E4DFB2832655C57E597CF83"
    # David Wicksell
    "74B3BE040ED458D6F32AADE46321D94F6FD1C8BB"
    # Srijan Pandey
    "7C9A96B7539C80903AE5FEB1F963F9C6E941578F"
    # Keziah Zapanta
    "729F7108E49E9AAF293EC5F06A06ABEB0DD4B446"
    # Peter Goss
    "583cdd3db91045a5eddda9b58ab10d34126a4839"
)
gpg --keyserver hkps://keyserver.ubuntu.com --recv-keys "${GPG_KEYS[@]}"

echo "# Add $upstream_repo as remote"
if ! git remote | grep -q upstream_repo; then
	git remote add upstream_repo "$upstream_repo"
	git fetch upstream_repo
else
	echo "Unable to add $upstream_repo as remote, remote name upstream_repo already exists"
	exit 1
fi

echo "target/upstream branch set to: $target_branch"

echo "# Fetch all commit ids only present in MR by comparing to target/upstream $target_branch branch"
COMMIT_IDS=`git rev-list upstream_repo/$target_branch..HEAD`

if [ -z "$COMMIT_IDS" ]; then
	# Only occurs when MR is merged and the pipeline execution is happening on upstream_repo
	COMMIT_IDS=`git rev-list HEAD~1..HEAD`
fi

echo "${COMMIT_IDS[@]}"

echo "# Verify commits and copyrights"

missing_files=""

for id in $COMMIT_IDS
do
	if ! git verify-commit "$id"; then
		echo "  --> Error: commit $id was not signed with a known GPG key!"
		exit 1
	fi
done

# Get file list from all commits at once, rather than per-commit
commit_list=$(echo $COMMIT_IDS | sed 's/$/ /g')
filelist="$(git show --pretty="" --name-only $commit_list | sort -u)"
curyear="$(date +%Y)"

for file in $filelist; do
	# Deleted files don't need a copyright notice, hence -e check
	if [ -e $file ] && $needs_copyright $file && ! grep -q 'Copyright (c) .*'$curyear' YottaDB LLC' $file; then
		# Print these out only at the end so they're all shown at once
		missing_files="$missing_files $file"
	fi
done

if [ -n "$missing_files" ]; then
	echo "  --> Error: some files are missing a YottaDB Copyright notice and/or current year $curyear"
	# Don't give duplicate errors for the same file
	for file in $(echo $missing_files | tr ' ' '\n' | sort -u); do
		echo "	$file"
	done
	exit 1
fi
