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
$! cmicom - compile/assemble cmi modules and place them in cmi.olb
$!	p1 = version number
$!	p2 = library (p, d, or b)
$!	p3 = target device and directory
$!
$ if p1 .eqs. ""
$  then
$   write sys$output "Must supply a version"
$   exit
$ endif
$ compopt=f$locate(p2,"PBD")
$ if (f$length(p2) .ne. 1) .or. (compopt .eq. 3)
$  then
$   write sys$output "Library must be P, B or D"
$   exit
$ endif
$ if p3 .eqs. ""
$  then
$   write sys$output "Must specify a target directory for the .olb file"
$   exit
$ endif
$!
$ alpha = (f$getsyi("arch_name") .eqs. "Alpha")
$ decc = alpha .or. (f$integer(f$extract(1,2,p1)) .ge. 32)
$ macopt = f$element(alpha,",",",/migration/flag=(hints)") + "/nolist" + f$element(compopt,",",",,/debug")
$! 2000/2/3 smw optimized cmi loops so turn off for now - 2000/2/8 fixed DEC C 6.2 ECO 2
$ compopt = "/include=gtm$src:/nolist" + f$element(compopt,",","/optimize,/optimize/define=debug,/nooptimize/define=debug/debug")
$! compopt = "/include=gtm$src:/nolist" + f$element(compopt,",","/nooptimize,/nooptimize/define=debug,/nooptimize/define=debug/debug")
$ compopt = "/standard=vaxc/assume=nowritable_string_literals" + compopt
$ ctlb :=
$ if alpha then ctlb = "+" + f$search("sys$library:sys$lib_c.tlb") + "/libr"
$ @gtm$tools:setactive_silent 'p1' 'p2'
$ calldir = f$environment("default")
$ set def 'p3'
$ x := message/nolist gtm$vrt:[cmi]cmierrors
$ if f$environment("VERIFY_PROCEDURE") then write sys$output x
$ x
$ x = "macro" + macopt + " gtm$vrt:[cmi]cmj_util.mar+" + f$search("gtm$src:maclib.mlb") + "/lib"
$ if f$environment("VERIFY_PROCEDURE") then write sys$output x
$ x
$ if .not. alpha
$  then
$   x := macro'macopt' gtm$vrt:[cmi]cmivector.mar
$   if f$environment("VERIFY_PROCEDURE") then write sys$output x
$   x
$ endif
$cloop:
$ fil = f$search("gtm$vrt:[cmi]*.c")
$ if fil .nes. ""
$  then
$   x = "cc"+compopt+" "+fil+ctlb
$   if f$environment("VERIFY_PROCEDURE") then write sys$output x
$   x
$   goto cloop
$ endif
$ gtm_library/create cmi
$ gtm_library cmi *
$ gtm_delete *.obj;*
$ gtm_purge cmi.olb
$ set def 'calldir'
$!
$ exit
