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
$! movempt.com - Move .mpt files from gtm$src to gtm$pct with a _ prefix
$!
$ if p1 .eqs. ""
$ then
$	write sys$output "Must supply a version"
$	exit
$ endif
$!
$ @gtm$tools:gtm_verify_symbols "set"	! sets the global symbols gtm_copy, gtm_delete, gtm_library, gtm_purge
$ @gtm$tools:build_print_stage "movempt" "begin"
$!
$ @gtm$tools:setactive_silent 'p1'
$ @gtm$tools:build_print_stage "Copying .mpt files to gtm$pct" "middle"
$
$ define/nolog gtm$pct gtm$root:['p1'.pct]
$ x = f$search("gtm$src:*.mpt")
$ if x .eqs. "" then $ goto nompt
$
$ set noon
$ gtm_delete gtm$pct:_*.m.*
$ set on
$
$ loop:
$	gtm_copy/prot=(s=re,o=rwed,g=re,w=re) 'x' gtm$pct:_'f$parse(x,,,"NAME")'.m
$	x = f$search("gtm$src:*.mpt")
$	if x .nes. "" then $ goto loop
$
$nompt:
$ if f$search("gtm$src:gtm$dmod.m") .nes. ""
$  then
$   gtm_copy/prot=(s=re,o=rwed,g=re,w=re) gtm$src:gtm$dmod.m gtm$pct:
$   gtm_purge gtm$pct:gtm$dmod.m
$ endif
$
$ if f$search("gtm$pct:*.obj") .nes. ""
$  then
$   set noon
$   set file/prot=(w=rwed) gtm$pct:*.obj
$   set on
$ endif
$
$ set command gtm$src:GTMCOMMANDS.CLDX	! define MUMPS command if .cldx file present
$ mumps/obj=gtm$pct: gtm$pct:*.m
$ gtm_purge gtm$pct:*.*
$
$ set noon
$ set file/prot=(s=re,w=re) gtm$pct:*.obj
$ set on
$!
$ @gtm$tools:build_print_stage "movempt" "end"
$ @gtm$tools:gtm_verify_symbols "unset"	! unsets the global symbols gtm_copy, gtm_delete gtm_library, gtm_purge
$ exit
