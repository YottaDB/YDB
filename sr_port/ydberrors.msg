!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
!								!
! Copyright (c) 2001-2017 Fidelity National Information		!
! Services, Inc. and/or its subsidiaries. All rights reserved.	!
!								!
! Copyright (c) 2017-2023 YottaDB LLC and/or its subsidiaries.	!
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
!  List of known undocumented messages follows (along with a comment) (do not change "known undocumented" phrase)
! ----- Buffer to introduce new undocumented error messages without affecting UNUSEDMSGnnn match with corresponding line numbers.
!
! NEW MESSAGES NEED TO BE ADDED AT THE END TO KEEP THE NUMBERING FOR THE CURRENT MESSAGES
!
! Note: This file is similar to sr_port/merrors.msg in layout.
!
QUERY2		<Invalid second argument to $QUERY. Must be -1 or 1.>/error/fao=0
MIXIMAGE	<Cannot load !AD image on process that already has !AD image loaded>/error/fao=4
LIBYOTTAMISMTCH	<$ydb_dist/libyottadb.so (!AD) does not match the shared library path (!AD)>/error/fao=4
READONLYNOSTATS	<Read-only and Statistics sharing cannot both be enabled on database>/error/fao=0
READONLYLKFAIL	<Failed to get !AD lock on READ_ONLY database file !AD>/error/fao=4
INVVARNAME	<Invalid local/global/ISV variable name !AD supplied to API call>/error/fao=2
PARAMINVALID	<!AD parameter specified in !AD call>/error/fao=4
INSUFFSUBS	<Return subscript array for !AD call too small - needs at least !UL entries for this call>/error/fao=3
MINNRSUBSCRIPTS	<Number of subscripts cannot be a negative number>/error/fao=0
SUBSARRAYNULL	<Non-zero number of subscripts [!UL] specified but subscript array parameter is NULL in !AD call>/error/fao=3
FATALERROR1	<Fatal error raised. Generating core and terminating process. Error: !AD>/fatal/fao=2
NAMECOUNT2HI	<Number of varnames specified as the namecount parameter in a !AD call (!UL) exceeds the maximum (!UL)>/error/fao=4
INVNAMECOUNT	<Number of varnames (namecount parameter in a !AD call) cannot be less than zero>/error/fao=2
FATALERROR2	<Fatal error raised. Bypassing core generation and terminating process. Error: !AD>/fatal/fao=2
TIME2LONG	<Specified time value [0x!16@XQ] exceeds supported maximum [0x!16@XQ]>/error/fao=2
VARNAME2LONG	<Variable name length exceeds maximum allowed (!UL)>/error/fao=1
SIMPLEAPINEST	<Attempt to nest call of !AZ with a call to !AZ - nesting calls is not permitted in the Simple API>/error/fao=2
CALLINTCOMMIT	<TCOMMIT at call-in-level=!UL not allowed as corresponding TSTART was done at lower call-in-level=!UL>/error/fao=2
CALLINTROLLBACK	<TROLLBACK at call-in-level=!UL not allowed as corresponding TSTART was done at lower call-in-level=!UL>/error/fao=2
TCPCONNTIMEOUT	<Connection wait timeout (!UL seconds) has expired>/error/fao=2
STDERRALREADYOPEN	<STDERR deviceparameter specifies an already open device !AD>/error/fao=2
SETENVFAIL	<VIEW "SETENV":"!AD" failed in setenv() system call>/error/fao=2
UNSETENVFAIL	<VIEW "UNSETENV":"!AD" failed in unsetenv() system call>/error/fao=2
UNKNOWNSYSERR	<[!UL] does not correspond to a known YottaDB error code>/error/fao=1
READLINEFILEPERM	<Readline history file !AZ could not be created>/warning/fao=1
NODEEND		<End of list of nodes/subscripts>/error/fao=0
READLINELONGLINE	<Entered line is greater than 32Kb long and will be truncated>/warning/fao=0
INVTPTRANS	<Invalid TP transaction - either invalid TP token or transaction not in progress>/error/fao=0
THREADEDAPINOTALLOWED	<Process cannot switch to using threaded Simple API while already using Simple API>/error/fao=0
SIMPLEAPINOTALLOWED	<Process cannot switch to using Simple API while already using threaded Simple API>/error/fao=0
STAPIFORKEXEC	<Calls to YottaDB are not supported after a fork() if threaded Simple API functions were in use in parent. Call exec() first>/error/fao=0
INVVALUE	<!AD is invalid !AD value for !AD>/error/fao=6
INVZCONVERT	<Translation supported only between DEC/HEX OR between UTF-8/UTF-16/UTF-16LE/UTF-16BE>/error/fao=0
ZYSQLNULLNOTVALID	<$ZYSQLNULL cannot be used as an integer, numeric, gvn subscript/value or lock subscript>/error/fao=0
BOOLEXPRTOODEEP	<Boolean expression depth exceeds maximum supported limit of 2047>/error/fao=0
TPCALLBACKINVRETVAL     <Invalid return type for TP callback function>/error/fao=0
INVMAINLANG	<Invalid main routine language id specified: !UL>/error/fao=1
WCSFLUFAILED	<!AD error while flushing buffers at transaction number 0x!16@XQ for database file !AD>/info/fao=5
WORDEXPFAILED	<wordexp() call for string [!AD] returned !AZ error. See wordexp() man pages for details>/error/fao=3
TRANSREPLJNL1GB	<Transaction can use at most 1GiB of replicated journal records across all journaled regions>/error/fao=0
DEVPARPARSE	<Error parsing device parameter specification>/error/fao=0
SETZDIRTOOLONG	<$ZDIR value specified is !UL bytes long which is greater than the allowed maximum of !UL bytes>/error/fao=2
UTF8NOTINSTALLED <$ydb_dist does not have utf8 folder installed. Please use M mode or re-install YottaDB with UTF-8 support>/error/fao=0
ISVUNSUPPORTED	<ISV variable name !AD not supported in !AD call>/error/fao=4
GVNUNSUPPORTED	<Global variable name !AD not supported in !AD call>/error/fao=4
ISVSUBSCRIPTED	<ISV variable name !AD specified with a non-zero subscript count of !UL>/error/fao=3
ZBRKCNTNEGATIVE	<Count [!SL], of transits through a ZBREAK breakpoint before activating it, cannot be negative>/error/fao=1
!
! If there are UNUSEDMSG* lines unused for more than one year and at least two non-patch releases, use them before adding new lines.
!
	.end
