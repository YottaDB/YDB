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
$! buildtcx - build gt.cx
$! parameters:
$!	p1 = version number
$!	p2 = library (p, d, or b)
$!	p3 = target device and directory
$!
$ if p1 .eqs. ""
$ then
$	write sys$output "Must supply a version"
$	exit
$ endif
$!
$ lnkimg=f$locate(p2,"PBD")
$ if (f$length(p2) .ne. 1) .or. (lnkimg .eq. 3)
$ then
$	write sys$output "Library must be P, B or D"
$	exit
$ endif
$!
$ if p3 .eqs. ""
$ then
$	write sys$output "Must specify a target directory for the .exe files"
$	exit
$ endif
$!
$ @gtm$tools:gtm_verify_symbols "set"	! sets the global symbols gtm_copy, gtm_delete, gtm_library, gtm_purge
$ @gtm$tools:build_print_stage "buildtcx" "begin"
$!
$! ---------- buildtcx set linker options -----------
$!
$ lnkopt = f$element(lnkimg,",","/notrace,/debug,/debug")
$ alpha = (f$getsyi("arch_name") .eqs. "Alpha")
$ if alpha then lnkopt = lnkopt + "/section"
$!
$ @gtm$tools:setactive_silent 'p1' 'p2'
$ set def 'p3'
$ targdir = f$parse("*.*",,,"device")+f$parse("*.*",,,"directory")
$ set def gtm$vrt
$ set def gtm$exe
$ set def [.obj]
$ objdir = f$parse("*.*",,,"device")+f$parse("*.*",,,"directory")
$ set def 'targdir'
$ set def [.map]
$ mapfile = f$parse("*.*",,,"device")+f$parse("*.*",,,"directory")
$ set def [-]
$!
$! ---------- buildtcx prepare options for the linker in .opt files -----------
$!
$ open/write relnam release_name.opt
$ write relnam "ident=",p1
$ close relnam
$!
$ open/write secshrlink secshrlink.opt
$ if ("" .eqs. f$trnlnm("gtm_no_secshr")) .or. (p2 .nes. "P")
$ then
$ 	write secshrlink "gtm$pro:gtmsecshr.exe/share"
$ endif
$ close secshrlink
$!
$! ---------- buildtcx ccp -----------
$!
$ @gtm$tools:build_print_stage "Building CCP" "middle"
$ define/user target 'targdir'
$ link/map='mapfile'CCP.map/exe='targdir'CCP.exe'lnkopt' 'objdir'mumps/lib/inc=CCP,sys$input/opt,target:secshrlink.opt/opt,target:release_name.opt/opt
name = CCP.EXE
$!
$! ---------- buildtcx cce -----------
$!
$ @gtm$tools:build_print_stage "Building CCE" "middle"
$ define/user target 'targdir'
$ link/map='mapfile'CCE.map/exe='targdir'CCE.exe'lnkopt' 'objdir'mumps/lib/inc=CCE,sys$input/opt,target:secshrlink.opt/opt,target:release_name.opt/opt
name = CCE.EXE
$!
$! ---------- buildtcx end -----------
$!
$ gtm_purge
$ gtm_purge [.map]*.*
$ gtm_delete release_name.opt;*,secshrlink.opt;*
$!
$ @gtm$tools:build_print_stage "buildtcx" "end"
$ @gtm$tools:gtm_verify_symbols "unset"	! unsets the global symbols gtm_copy, gtm_delete gtm_library, gtm_purge
$ exit
