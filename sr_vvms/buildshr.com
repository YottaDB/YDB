$!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
$!								!
$!	Copyright 2001, 2012 Fidelity Information Services, Inc	!
$!								!
$!	This source code contains the intellectual property	!
$!	of its copyright holder(s), and is made available	!
$!	under a license.  If you do not know the terms of	!
$!	the license, please stop and do not read further.	!
$!								!
$!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
$!
$! buildshr - build a shareable image
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
$ @gtm$tools:build_print_stage "buildshr" "begin"
$!
$! ---------- buildshr set linker options -----------
$!
$ lnkopt = f$element(lnkimg,",","/notrace,/debug,/debug")
$ alpha = (f$getsyi("arch_name") .eqs. "Alpha")
$ if alpha then lnkopt = lnkopt + "/section"
$!
$! smw 2001/5/2 force no dsf files for now - later check axp and logical
$ dsffiles = 0
$!
$ @gtm$tools:setactive_silent 'p1' 'p2'
$ set def gtm$vrt
$ set def gtm$exe
$ if f$search("gtmshr.olb") .eqs. "" then gtm_library/create gtmshr.olb
$ if f$search("gtmlib.olb") .eqs. "" then gtm_library/create gtmlib.olb
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
$! ---------- buildshr prepare options for the linker in .opt files -----------
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
$! ---------- buildshr gtmshr (after determining filler) -----------
$!
$ fillname = ""
$!
$ if (lnkimg .eq. 0) .and. ((f$extract(0,2,p1) .nes. "V9") .or. (p1 .eqs. "V999"))	! keep the pro image size stable
$ then
$   @gtm$tools:build_print_stage "Building template GTMSHR" "middle"
$   @gtm$tools:linkshr 'targdir' 'objdir' 'lnkopt'/share=nl:/map=sizemap.tmp/brief ""
$   search/out=temp.tmp sizemap.tmp "virtual memory allocated"
$   if 1 .ne. $severity
$    then
$     write sys$output "%GTM-F-BUILDCHK, Check map (sizemap.tmp) format"
$     @gtm$tools:gtm_verify_symbols "unset"	! unsets the global symbols gtm_copy, gtm_delete gtm_library, gtm_purge
$     exit
$   endif
$   open/read temp temp.tmp
$   read temp a
$   close temp
$   x = f$locate(",",a) + 1
$   a = f$extract(x,99,a)
$   image_size = f$extract(1,f$locate(".",a)-1,a)
$   if 0 .eq. f$integer(image_size)
$    then
$     write sys$output "%GTM-F-BUILDCHK, Check map (sizemap.tmp) virtual memory pages"
$     @gtm$tools:gtm_verify_symbols "unset"	! unsets the global symbols gtm_copy, gtm_delete gtm_library, gtm_purge
$     exit
$   endif
$   gtm_delete temp.tmp;*,sizemap.tmp;*
$   if alpha
$     then
$!	Note: pages are 512 bytes
$       page_target = 6000
$       if f$extract(1,4,p1) .eqs. "60FT" .or. f$extract(1,4,p1) .eqs. "60BL"
$	    then
$		if f$extract(5,2,p1) .lts. "09" then $ page_target = 5120
$	endif
$       if f$extract(1,5,p1) .les. "60000" then $ page_target = 5120
$       if f$extract(1,2,p1) .lts. "43" then $ page_target = 4000
$     else
$       page_target = 2000
$       if f$extract(0,3,p1) .eqs. "V42" then $ page_target = 1600
$       if f$extract(0,3,p1) .eqs. "V40" then $ page_target = 1200
$       if f$extract(0,3,p1) .eqs. "V32" then $ page_target = 1200
$       if f$extract(0,3,p1) .eqs. "V31" then $ page_target = 1000
$       if f$extract(0,2,p1) .eqs. "V2" then $ page_target = 950
$   endif
$   if image_size .lt. page_target
$    then
$     fillname = "gtmfiller." + f$element(alpha,",","mar,m64")
$     create 'fillname'
	.title	gtmfiller - patch & expansion space
	.psect	gtmfiller,con,nord,noexe,nowrt,pic,shr,gbl
$     open/append fill 'fillname'
$     write fill "page_target = ",page_target
$     write fill "actual_pages = ",image_size
$     write fill "	.blkb	<page_target - actual_pages> * 512"
$     write fill "	.end"
$     close fill
$     if f$environment("VERIFY_PROCEDURE") then write sys$output "[BUILDSHR-I-IMAGESIZE Actual size ",image_size," pages]"
$     qual = f$element(alpha,",",",/alpha")
$     fillname := gtmfiller
$     macro'qual'/nolist gtmfiller
$   else
$     write sys$output "%BUILDSHR-W-IMAGESIZE Image size (",image_size,") is greater than target size of ",page_target
$   endif
$ endif
$!
$ @gtm$tools:build_print_stage "Building real GTMSHR" "middle"
$ gtmshrdsf = ""
$ if dsffiles then gtmshrdsf = "/dsf=gtmshr.dsf"
$ @gtm$tools:linkshr 'targdir' 'objdir' 'lnkopt'/symb='mapfile'gtmshr.stb/share=gtmshr.exe/map='mapfile'gtmshr.map/full'gtmshrdsf' 'fillname'
$ if fillname .nes. "" then $ gtm_delete gtmfiller.obj;*,gtmfiller.m%%;*
$!
$! ---------- buildshr mcompile -----------
$!
$ @gtm$tools:build_print_stage "Building MCOMPILE" "middle"
$ define/user objlib 'objdir'mumps.olb
$ define/user target 'targdir'
$ set def 'targdir'
$ mcompdsf = ""
$ if dsffiles then mcompdsf = "/dsf=mcompile.dsf"
$ link/exe=mcompile.exe/map=[.map]mcompile.map/full'lnkopt' 'mcompdsf' objlib/incl=mcompile,sys$input/opt,target:release_name/opt
stack = 256
name = MCOMPILE.EXE
target:gtmshr.exe/share
$!
$! ---------- buildshr create gtmshr.olb and gtmlib.olb -----------
$!
$ @gtm$tools:build_print_stage "Creating GTMSHR.OLB and GTMLIB.OLB" "middle"
$ gtm_library/create=(block:5,hist:0,modu:3,glob:10)/share gtmshr.olb
$ gtm_library gtmshr.olb gtmshr.exe
$ gtm_library/create=(block:5,hist:0,modu:3,glob:10) gtmlib.olb
$ gtm_library/extract=(gtm_main,gtm$defaults,gtmi$def)/out=gtm_main 'objdir'mumps
$ gtm_library gtmlib.olb gtm_main.obj
$ set noon
$ gtm_library/extract=gtm_zc/out=gtm_zc 'objdir'mumps
$ severity = $severity
$ set on
$ if severity .eq. 1
$  then
$   gtm_library gtmlib.olb gtm_zc
$   gtm_delete gtm_zc.obj;*
$ endif
$!
$! ---------- buildshr gtm$dmod -----------
$!
$ @gtm$tools:build_print_stage "Building GTM$DMOD" "middle"
$!
$! use production images to compile GTM$DMOD.M
$ @gtm$tools:setactive_silent 'p1' p
$ set def [.obj]
$ set command gtm$src:GTMCOMMANDS.CLDX	! define MUMPS command if .cldx file present
$ mumps gtm$src:gtm$dmod.m
$ gtm_library /replace mumps gtm$dmod.obj
$ gtm_delete gtm$dmod.obj;
$ set def [-]
$ @gtm$tools:setactive_silent 'p1' 'p2' ! revert back to currently building image
$!
$ dmoddsf = ""
$ if dsffiles then dmoddsf = "/dsf=gtm$dmod.dsf"
$ define/user target 'targdir'
$ link/map=[.map]gtm$dmod.map/full/exe=gtm$dmod.exe'lnkopt' 'dmoddsf' 'objdir'mumps/includ=gtm$dmod,sys$input/opt,target:release_name/opt
name = GTM$DMOD.EXE
$!
$! ---------- buildshr end -----------
$!
$ gtm_delete gtm_main.obj.*,release_name.opt;*,secshrlink.opt;*
$ @gtm$tools:build_print_stage "buildshr" "end"
$!
$ @gtm$tools:gtm_verify_symbols "unset"	! unsets the global symbols gtm_copy, gtm_delete gtm_library, gtm_purge
$ exit
