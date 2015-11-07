$!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
$!								!
$!	Copyright 2001, 2013 Fidelity Information Services, Inc	!
$!								!
$!	This source code contains the intellectual property	!
$!	of its copyright holder(s), and is made available	!
$!	under a license.  If you do not know the terms of	!
$!	the license, please stop and do not read further.	!
$!								!
$!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
$!
$! buildsec - build gtmsecshr
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
$ @gtm$tools:build_print_stage "buildsec" "begin"
$!
$! ---------- set linker options -----------
$!
$ lnkopt = ""
$ alpha = (f$getsyi("arch_name") .eqs. "Alpha")
$ if alpha then lnkopt = lnkopt + "/section"
$!
$! smw 2001/5/2 force no dsf files for now - later check axp and logical
$ dsffiles = 0
$!
$ @gtm$tools:setactive_silent 'p1' 'p2'
$ set def 'p3'
$ targdir = f$parse("*.*",,,"device")+f$parse("*.*",,,"directory")
$ set def gtm$vrt
$ set def gtm$exe
$ if f$search("gtmshr.olb") .eqs. "" then gtm_library/create gtmshr.olb
$ if f$search("gtmlib.olb") .eqs. "" then gtm_library/create gtmlib.olb
$ set def [.obj]
$ objdir = f$parse("*.*",,,"device")+f$parse("*.*",,,"directory")
$ define/nolog obd 'objdir'mumps.olb	! used by secshrlink.axp
$ define/nolog urd 'objdir'user_rundown.obj
$ gtm_library /extract=user_rundown /out=user_rundown mumps.olb
$ set def 'targdir'
$ set def [.map]
$ mapfile = f$parse("*.*",,,"device")+f$parse("*.*",,,"directory")
$ set def [-]
$!
$! ---------- prepare options for the linker in .opt files -----------
$!
$ open/write relnam release_name.opt
$ write relnam "ident=",p1
$ close relnam
$!
$! ---------- build gtmsecshr -----------
$!
$ @gtm$tools:build_print_stage "Building GTMSECSHR" "middle"
$ secshrlink = "secshrlink." + f$element(alpha,",","vax,axp")
$ gtmsecshrdsf = ""
$ if dsffiles then gtmsecshrdsf = "/dsf=gtmsecshr.dsf"
$ define/user target 'targdir'
$ link /protect/notrace/share='targdir'gtmsecshr.exe 'gtmsecshrdsf' 'lnkopt' -
       /map='mapfile'gtmsecshr.map/full gtm$tools:'secshrlink'/opt,sys$input/opt,target:release_name.opt/opt
name = GTMSECSHR.EXE
$ set prot=(o:rewd,s:rewd,g:re,w:re) 'targdir'gtmsecshr.exe
$!
$! ---------- build crashandburn -----------
$!
$ @gtm$tools:build_print_stage "Building CRASHANDBURN" "middle"
$ ccopts := /standard=vaxc/share_globals/float=g_float/warn=disable=(signedknown,signedmember,questcompare,questcompare1)
$ ccopts = ccopts + "/inc=(gtm$src:,tcpip$examples:)/assume=nowritable_string_literals/nolist/define=(TEST_REPL"
$ if "P" .nes. f$edit(f$extract(0, 1, p2), "UPCASE") then $ ccopts = ccopts + ",DEBUG"
$ ccopts = ccopts + ")"
$ if "D" .eqs. f$edit(f$extract(0, 1, p2), "UPCASE") then $ ccopts = ccopts + "/nooptimize/debug"
$ cc'ccopts'/object=urd gtm$src:USER_RUNDOWN.C
$ define/user target 'targdir'
$ link /protect/notrace/share='targdir'crashandburn.exe 'lnkopt' -
       /map='mapfile'crashandburn.map/full gtm$tools:'secshrlink'/opt,sys$input/opt,target:release_name.opt/opt
name = CRASHANDBURN.EXE
$ if (f$parse(targdir,,,,"NO_CONCEAL") - "][") .eqs. (f$parse("GTM$ROOT:[" + P1 + ".PRO]",,,,"NO_CONCEAL") - "][")
$  then
$   newsec = p1 + "_gtmsecshr.exe"
$   newburn = p1 + "_crashandburn.exe"
$   gtm_copy gtmsecshr.exe 'newsec'
$   gtm_copy crashandburn.exe 'newburn'
$   if dsffiles
$   then
$     newdsf = p1 + "_gtmsecshr.dsf"
$     gtm_copy gtmsecshr.dsf 'newdsf'
$   endif
$   curpriv=f$setprv("bypas")
$   gtm_copy 'newsec' gtm$sec:
$   curpriv=f$setprv(curpriv)
$   curpriv=f$setprv("cmkrnl")
$   install replace/header/share/protect/open gtm$sec:'newsec'
$
$!  install remove older version 'p1'_crashandburn.exe in case it was previously installed
$   define/user sys$output nl:
$   define/user sys$error  nl:
$   install remove gtm$root:['p1'.pro]'newburn'
$
$   curpriv=f$setprv(curpriv)
$   newsec = newsec + ","
$  else
$   newsec :=
$ endif
$ gtm_delete 'f$trnlnm("urd").
$!
$! pur/log 'targdir''newsec'gtmsecshr.exe,'mapfile'gtmsecshr.map
$!
$ @gtm$tools:build_print_stage "buildsec" "end"
$ @gtm$tools:gtm_verify_symbols "unset"	! unsets the global symbols gtm_copy, gtm_delete gtm_library, gtm_purge
$ exit
