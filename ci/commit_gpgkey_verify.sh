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

error_check() {
	msg="$1"
	result="$2"
	if [ $result -ne 0 ]; then
		echo $msg
		exit 1
	fi
}

if [ $# -eq 0 ] || [ -z "$1" ]; then
	echo "Need to pass target/upstream project URL as the first argument"
	exit 1
fi

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
)
gpg --keyserver hkps://keyserver.ubuntu.com --recv-keys "${GPG_KEYS[@]}"

echo "# Add $1 as remote"
if ! git remote | grep -q upstream_repo; then
	git remote add upstream_repo "$1"
	error_check "git remote add failed for upstream_repo $1" $?
	git fetch upstream_repo
	error_check "git fetch failed for upstream_repo" $?
else
	echo "Unable to add $1 as remote, remote name upstream_repo already exists"
	exit 1
fi

echo "# Set target/upstream branch"
if [ -z "$2" ]; then
	ydb_branch=master
else
	ydb_branch="$2"
fi
echo "target/upstream branch set to: $ydb_branch"

echo "# Fetch all commit ids only present in MR by comparing to target/upstream $ydb_branch branch"
COMMIT_IDS=`git rev-list upstream_repo/$ydb_branch..HEAD`
error_check "failed to fetch commit" $?

if [ -z "$COMMIT_IDS" ]; then
	# Only occurs when MR is merged and the pipeline execution is happening on upstream_repo
	COMMIT_IDS=`git rev-list HEAD~1..HEAD`
	error_check "failed to fetch HEAD commit" $?
fi

echo "${COMMIT_IDS[@]}"

echo "# Verify commits"
for id in $COMMIT_IDS
do
	if ! git verify-commit "$id"; then
		echo " -> The commit $id was not signed with a known GPG key!"
		exit 1
	fi
done
