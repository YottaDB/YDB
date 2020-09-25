#!/usr/bin/env bash

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
)
gpg --keyserver hkps://keyserver.ubuntu.com --recv-keys "${GPG_KEYS[@]}"
# verify-commit was only introduced in 2.5: https://stackoverflow.com/a/32038784/7669110
# If git is not recent enough for the check, just skip it
MAJOR_GIT_VERSION="$(git --version | cut -d ' ' -f 3 | cut -d '.' -f 1)"
if [ $MAJOR_GIT_VERSION -ge 2 ] && ! git verify-commit HEAD; then
    echo " -> The commit was not signed with a known GPG key!"
    exit 1
fi


