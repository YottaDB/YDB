#!/bin/bash
#################################################################
#                                                               #
# Copyright (c) 2023 YottaDB LLC and/or its subsidiaries.  #
# All rights reserved.                                          #
#                                                               #
#       This source code contains the intellectual property     #
#       of its copyright holder(s), and is made available       #
#       under a license.  If you do not know the terms of       #
#       the license, please stop and do not read further.       #
#                                                               #
#################################################################
timestamp=$(date) # timestamp for logs in case needed for troubleshooting
. /opt/yottadb/current/ydb_env_set
source $HOME/.cargo/env
mkdir -p logs
if ! yottadb -run %ydboctoAdmin show users | grep -qw ydb  ; then
    echo $timestamp >>logs/%ydboctoAdmin.log
    printf "ydbrocks\nydbrocks" | "$ydb_dist/yottadb" -r %ydboctoAdmin add user ydb -w -a >>logs/%ydboctoAdmin.log
fi
if [ ! -e "node_modules" ] ; then
    echo $timestamp >>logs/npm.log
    npm install nodem 1>>logs/npm.log 2>&1
fi
echo $timestamp >>logs/rocto.log
rocto -aw 2>>logs/rocto.log &
echo $timestamp >>logs/%ydbgui.log
yottadb -run %ydbgui --readwrite >>logs/%ydbgui.log &
exec /bin/bash
