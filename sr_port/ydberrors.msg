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
! Note: This file is similar to sr_port/merrors.msg in layout.
!
QUERY2		<Invalid second argument to $QUERY. Must be -1 or 1.>/error/fao=0
MIXIMAGE	<Cannot load !AD image on process that already has !AD image loaded>/error/fao=4
LIBYOTTAMISMTCH	<$ydb_dist/libyottadb.so (!AD) does not match the shared library path (!AD)>/error/fao=4!/ansi=0
READONLYNOSTATS	<Read-only and Statistics sharing cannot both be enabled on database>/error/fao=0!/ansi=0
READONLYLKFAIL	<Failed to get !AD lock on READ_ONLY database file !AD>/error/fao=4!/ansi=0
INVVARNAME	<Invalid local/global/ISV variable name supplied to API call>/error/fao=0
PARAMINVALID	<!AD parameter specified in !AD call>/error/fao=4
INSUFFSUBS	<Return subscript array for !AD call too small - needs at least !UL entries for this call>/error/fao=3
MINNRSUBSCRIPTS	<Number of subscripts cannot be a negative number>/error/fao=0
SUBSARRAYNULL	<Non-zero number of subscripts [!UL] specified but subscript array parameter is NULL in !AD call>/error/fao=3
FATALERROR1	<Fatal error raised. Generating core and terminating process. Error: !AD>/fatal/fao=2
NAMECOUNT2HI	<Number of varnames (namecount parameter in a !AD call) exceeds maximum (!UL)>/error/fao=3
INVNAMECOUNT	<Number of varnames (namecount parameter in a !AD call) cannot be less than zero>/error/fao=2
FATALERROR2	<Fatal error raised. Bypassing core generation and terminating process. Error: !AD>/fatal/fao=2
TIME2LONG	<Specified time value [0x!16@XQ] exceeds supported maximum [0x!16@XQ]>/error/fao=2
VARNAME2LONG	<Variable name length exceeds maximum allowed (!UL)>/error/fao=1
SIMPLEAPINEST	<Attempt to nest call of !AD with a call to !AD - nesting calls is not permitted in simpleAPI>/error/fao=4
CALLINTCOMMIT	<TCOMMIT at call-in-level=!UL not allowed as corresponding TSTART was done at lower call-in-level=!UL>/error/fao=2!/ansi=0
CALLINTROLLBACK	<TROLLBACK at call-in-level=!UL not allowed as corresponding TSTART was done at lower call-in-level=!UL>/error/fao=2!/ansi=0
