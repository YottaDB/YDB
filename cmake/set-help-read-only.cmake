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

set(ENV{ydb_dist} ${YDB_INSTALL_DIR})
set(ENV{ydb_routines} .)
set(ENV{ydb_gbldir} ${YDB_INSTALL_DIR}/${help}help)

message(STATUS "Setting ${help}help to read-only")
message(TRACE "Running ydb_dist=${YDB_INSTALL_DIR} ydb_gbldir=${YDB_INSTALL_DIR}/${help}help ${YDB_INSTALL_DIR}/mupip SET -ACC=MM -READ_ONLY -reg *")
execute_process(COMMAND ${YDB_INSTALL_DIR}/mupip SET -ACC=MM -READ_ONLY -reg * WORKING_DIRECTORY ${YDB_INSTALL_DIR})
