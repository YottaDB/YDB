$!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
$!								!
$!	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	!
$!								!
$!	This source code contains the intellectual property	!
$!	of its copyright holder(s), and is made available	!
$!	under a license.  If you do not know the terms of	!
$!	the license, please stop and do not read further.	!
$!								!
$!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
$!
$! full build comes through axp_comlist.com which would have done set verify where the more output the merrier
$! but for incremental invocations of this script, we do not want lots of output spewed so avoid logging in that case.
$! since we cannot pass a local symbol to the caller, we set a global symbol.
$!
$! p1 - "set" or "unset"
$!
$ if (p1 .eqs. "set")
$ then
$	if f$environment("VERIFY_PROCEDURE")
$	then
$		gtm_copy    :== copy/log
$		gtm_delete  :== delete/log
$		gtm_library :== library/log
$		gtm_purge   :== purge/log
$	else
$		gtm_copy    :== copy/nolog
$		gtm_delete  :== delete/nolog
$		gtm_library :== library/nolog
$		gtm_purge   :== purge/nolog
$	endif
$ else
$	gtm_del = "delete"	! in case someone has redefined "delete"
$	gtm_del /symb/glob gtm_copy
$	gtm_del /symb/glob gtm_delete
$	gtm_del /symb/glob gtm_library
$	gtm_del /symb/glob gtm_purge
$ endif
