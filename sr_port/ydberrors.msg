!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
!								!
! Copyright (c) 2001-2017 Fidelity National Information		!
! Services, Inc. and/or its subsidiaries. All rights reserved.	!
!								!
! Copyright (c) 2017-2018 YottaDB LLC. and/or its subsidiaries.	!
! All rights reserved.						!
!								!
!	This source code contains the intellectual property	!
!	of its copyright holder(s), and is made available	!
!	under a license.  If you do not know the terms of	!
!	the license, please stop and do not read further.	!
!								!
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

! Code in this module is based on merrors.msg and hence has an
! FIS copyright even though this module was not created by FIS.

	.FACILITY	YDB,256/PREFIX=ERR_
	.TITLE		YDBERRORS Error Messages for YottaDB
!
! Note: This file is similar to sr_port/merrors.msg in layout
!
QUERY2		<Invalid second argument to $QUERY. Must be -1 or 1.>/error/fao=0
MIXIMAGE	<Cannot load !AD image on process that already has !AD image loaded>/error/fao=4
LIBYOTTAMISMTCH	<$ydb_dist/libyottadb.so (!AD) does not match the shared library path (!AD)>/error/fao=4!/ansi=0
READONLYNOSTATS	<Read-only and Statistics sharing cannot both be enabled on database>/error/fao=0!/ansi=0
READONLYLKFAIL	<Failed to get !AD lock on READ_ONLY database file !AD>/error/fao=4!/ansi=0
VARNAMEINVALID	<Invalid local/global/ISV variable name supplied to API call>/error/fao=0
PARAMINVALID	<!AD parameter specified in !AD call>/error/fao=4
INSUFFSUBS	<Return subscript array for !AD call too small - needs at least !UL entries for this call>/error/fao=3
MINNRSUBSCRIPTS	<Number of subscripts cannot be a negative number>/error/fao=0
SUBSARRAYNULL	<Non-zero number of subscripts [!UL] specified but subscript array parameter is NULL in !AD call>/error/fao=3
