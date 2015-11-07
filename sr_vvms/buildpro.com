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
$! buildpro.com - build pro images p1 = version number; p2 = VMS version for libraries
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
$ @gtm$tools:buildsec 'p1' p gtm$root:['p1'.pro]
$ @gtm$tools:buildshr 'p1' p gtm$root:['p1'.pro]
$ @gtm$tools:movempt 'p1'
$ @gtm$tools:buildaux 'p1' p gtm$root:['p1'.pro]
$!
$ minimal = f$trnlnm("minimal_build")
$ if (minimal .eqs. "")
$ then
$	@gtm$tools:buildtcx 'p1' p gtm$root:['p1'.pro]
$ endif
$!
$ @gtm$tools:gtm_verify_symbols "set"	! sets the global symbols gtm_copy, gtm_delete, gtm_library, gtm_purge
$!
$ if (f$getsyi("arch_name") .eqs. "Alpha") then $ @gtm$tools:srm_check gtm$root:['p1'.pro]
$ dir/nohead/notrail/ver=1/out=gtm$root:['p1']src.txt gtm$src
$ gtm_purge gtm$root:['p1']src.txt
$ set def gtm$root:['p1'.pro]
$ gtm_copy gtm$src:gtmcommands.cldx []gtmcommands.cld
$ gtm_copy gtm$src:gtm$defaults.* gtm$vrt:[tls]
$ gtm_purge gtm$vrt:[tls]
$ gtm_library/create=(block:25)/macro gtmzcall
$ gtm_library/macro gtmzcall gtm$src:gtmzcall.max
$ set noon
$ gtm_library/extract=mumps_binding/output=mumps_binding.max gtm$vrt:[pro.obj]maclib.mlb
$ if $severity .eq. 1
$  then
$   gtm_library/macro gtmzcall mumps_binding.max
$   gtm_delete mumps_binding.max.
$ endif
$ gtm_library/out=gtm$vrt:[pro.obj]xref.lis/cross gtm$vrt:[pro.obj]mumps
$ set def gtm$pro
$ gtm_purge gtm$root:['p1'.pro]
$ set prot=w:re gtm$vrt:[pro]*.exe
$ set prot=w:r gtm$vrt:[pro]*.olb
$ if old then $ @gtm$tools:define-old-library-logicals remove
$!
$ @gtm$tools:gtm_verify_symbols "unset"	! unsets the global symbols gtm_copy, gtm_delete gtm_library, gtm_purge
$ exit
