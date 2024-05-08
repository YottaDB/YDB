#!/bin/bash
#################################################################
#                                                               #
# Copyright (c) 2024 YottaDB LLC and/or its subsidiaries.  #
# All rights reserved.                                          #
#                                                               #
#       This source code contains the intellectual property     #
#       of its copyright holder(s), and is made available       #
#       under a license.  If you do not know the terms of       #
#       the license, please stop and do not read further.       #
#                                                               #
#################################################################
mkdir -p /data/logs
. /opt/yottadb/current/ydb_env_set
yottadb -run %ydbgui --readwrite --port 9080 >>/data/logs/%ydbgui.log &
exec /opt/yottadb/current/yottadb -direct
