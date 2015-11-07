$!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
$!								!
$!	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	!
$!								!
$!	This source code contains the intellectual property	!
$!	of its copyright holder(s), and is made available	!
$!	under a license.  If you do not know the terms of	!
$!	the license, please stop and do not read further.	!
$!								!
$!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
$!
$! builddbg.com build dbg images - p1 = version number; p2 = VMS version for libraries
$!
$ if p1 .eqs. "" then $exit
$ old = 0
$ vmsver = f$extract(1,3,f$getsyi("version")) - "."
$ if (f$extract(0,1,p2) .eqs. "V") .and. (f$extract(1,2,p2) .nes. vmsver)
$  then
$   old = 1
$   @gtm$tools:define-old-library-logicals 'p2'
$ endif
$ @gtm$tools:buildshr 'p1' d gtm$root:['p1'.dbg]
$ @gtm$tools:buildaux 'p1' d gtm$root:['p1'.dbg]
$!
$ minimal = f$trnlnm("minimal_build")
$ if (minimal .eqs. "")
$ then
$	@gtm$tools:buildtcx 'p1' d gtm$root:['p1'.dbg]
$ endif
$ if (f$getsyi("arch_name") .eqs. "Alpha") then $ @gtm$tools:srm_check gtm$root:['p1'.dbg]
$ if old then $ @gtm$tools:define-old-library-logicals remove
$ exit
