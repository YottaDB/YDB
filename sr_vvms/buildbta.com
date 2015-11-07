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
$! buildbta.com - build bta images p1=version number; p2 = VMS version for libraries
$!
$ if p1 .eqs. ""
$ then
$	write sys$output "Must supply a version"
$	exit
$ endif
$!
$ old = 0
$ vmsver = f$extract(1,3,f$getsyi("version")) - "."
$ if (f$extract(0,1,p2) .eqs. "V") .and. (f$extract(1,2,p2) .nes. vmsver)
$  then
$   old = 1
$   @gtm$tools:define-old-library-logicals 'p2'
$ endif
$ @gtm$tools:buildshr 'p1' b gtm$root:['p1'.bta]
$ if f$search("gtm$vrt:[pct]_dh.obj") .eqs. "" then @gtm$tools:movempt 'p1'
$ @gtm$tools:buildaux 'p1' b gtm$root:['p1'.bta]
$!
$ minimal = f$trnlnm("minimal_build")
$ if (minimal .eqs. "")
$ then
$	@gtm$tools:buildtcx 'p1' b gtm$root:['p1'.bta]
$ endif
$ if (f$getsyi("arch_name") .eqs. "Alpha") then $ @gtm$tools:srm_check gtm$root:['p1'.bta]
$!
$ @gtm$tools:gtm_verify_symbols "set"	! sets the global symbols gtm_copy, gtm_delete, gtm_library, gtm_purge
$!
$! libr/out=gtm$vrt:[bta.obj]xref.lis/cross gtm$vrt:[bta.obj]mumps
$ gtm_purge gtm$vrt:[bta.obj]
$ set prot=w:re gtm$vrt:[bta]*.exe
$ set prot=w:r gtm$vrt:[bta]*.olb
$ if old then $ @gtm$tools:define-old-library-logicals remove
$!
$ @gtm$tools:gtm_verify_symbols "unset"	! unsets the global symbols gtm_copy, gtm_delete gtm_library, gtm_purge
$ exit
