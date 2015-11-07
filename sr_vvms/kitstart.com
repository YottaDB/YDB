$!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
$!								!
$!	Copyright 2001, 2013 Fidelity Information Services Inc	!
$!								!
$!	This source code contains the intellectual property	!
$!	of its copyright holder(s), and is made available	!
$!	under a license.  If you do not know the terms of	!
$!	the license, please stop and do not read further.	!
$!								!
$!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
$!
$! kitstart.com p1=product p2=version, p3=destination
$!
$ alpha = (f$getsyi("arch_name") .eqs. "Alpha")
$ if alpha
$  then
$   prodlist = "GT.M\GT.CM\GT.CX\GT.DDP\GT.DC\ALL\"
$  else
$   prodlist = "GT.M\GT.CM\GT.CX\GTM.FI\GT.DDP\ALL\"
$ endif
$ pllen = f$length(prodlist)
$ gtm_init_tape :== y
$ say = "write sys$output"
$ ask = "inquire"
$ say "GT.M Distribution Kit Build"
$ on error then goto badkit
$ on severe then goto badkit
$ on control_y then goto badkit
$ prd = p1
$ p1 = ""
$ vno = p2
$ p2 = ""
$ dst = p3
$ p3 = ""
$ gosub getprod
$ gosub getver
$ gosub getdst
$ ver 'vno' p
$ savpriv = f$setprv("bypas,cmkrnl,log_io,sysnam,volpro,oper")
$ if f$getdvi(outdev,"devclass") .eq. 2
$  then
$   init/over=(accessibility,expiration,owner)/density=1600 'outdev' gtc
$ endif
$! Fix the Version #s in "gtm$vrt:[t%%]*_spkitbld.dat"
$ @gtm$tools:spkitupdate.com 'vno'
$!
$ if (f$locate("GT.M",prd) + f$locate("ALL",prd)) .ne. prdlen2
$  then
$   @sys$update:spkitbld "" 'dst' "" gtm$vrt:[tls]gtm_spkitbld.dat
$   if $severity .ne. 1 then goto badkit
$   prot_obj_files="(S:RE,O:RWED,G:RE,W:RE)"
$   f="GTM$VRT:[PCT]*.obj"
$   write sys$output "    Resetting protection on all obj files to ''prot_obj_files' ..."
$   set security/class=file/protection='prot_obj_files' 'f'
$   gtm_init_tape :== n
$ endif
$ if (f$locate("GT.CM",prd) + f$locate("ALL",prd)) .ne. prdlen2
$  then
$   @sys$update:spkitbld "" 'dst' "" gtm$vrt:[tcm]gtcm_spkitbld.dat
$   if $severity .ne. 1 then goto badkit
$   gtm_init_tape :== n
$ endif
$!1999/1/3 smw CX not supported if (f$locate("GT.CX",prd) + f$locate("ALL",prd)) .ne. prdlen2
$ if (f$locate("GT.CX",prd)) .ne. f$length(prd)
$  then
$   @sys$update:spkitbld "" 'dst' "" gtm$vrt:[tcx]gtcx_spkitbld.dat
$   if $severity .ne. 1 then goto badkit
$   gtm_init_tape :== n
$ endif
$ if .NOT. alpha .AND. (f$locate("GTM.FI",prd) + f$locate("ALL",prd)) .ne. prdlen2
$  then
$   @sys$update:spkitbld "" 'dst' "" gtm$vrt:[tfi]gtmfi_spkitbld.dat
$   if $severity .ne. 1 then goto badkit
$   gtm_init_tape :== n
$ endif
$ if (f$locate("GT.DDP",prd) + f$locate("ALL",prd)) .ne. prdlen2
$  then
$   @sys$update:spkitbld "" 'dst' "" gtm$vrt:[tdp]ddp_spkitbld.dat
$   if $severity .ne. 1 then goto badkit
$   gtm_init_tape :== n
$ endif
$ if (f$locate("GT.DC",prd) + f$locate("ALL",prd)) .ne. prdlen2
$  then
$   @sys$update:spkitbld "" 'dst' "" gtm$vrt:[tdc]gtmdc_spkitbld.dat
$   if $severity .ne. 1 then goto badkit
$   gtm_init_tape :== n
$ endif
$ if f$getdvi(outdev,"DEVCLASS") .eq. 2
$  then
$   if .not. f$getdvi(outdev,"MNT") then mount/foreign/noassist 'outdev'
$   dismount/unload 'outdev'
$ endif
$ @gtm$tools:kitprepare.com 'vno'
$goodbye:
$ savpriv = f$setprv(savpriv)
$ exit
$!
$badkit:
$type sys$input:

	+---------------------+
	|		      |
	|  THIS IS A BAD KIT  |
	|		      |
	+---------------------+

$ goto goodbye
$!
$getprod:
$ prdlen2 = f$length(prd) * 2
$ if prdlen2 .eq. 0
$  then
$ 	ask prd "Product"
$ 	if f$extract(0,1,prd) .eqs. "Q" then exit
$ 	goto getprod
$ endif
$ n = 0
$prodloop:
$ t1 = f$element(n,",",prd)
$ if t1 .eqs. "," then return
$ t1 = f$locate(t1,prodlist)
$ if t1 .eq. pllen
$  then
$ 	say "Product not available"
$ 	prd = ""
$ 	goto getprod
$ endif
$ n = n + 1
$ goto prodloop
$!
$getver:
$ if f$search("gtm$root:["+vno+".pro]gtmshr.exe") .nes. "" then return
$ if vno .nes. "" then say "Version Number not available"
$ ask vno "Version Number"
$ if f$extract(0,1,vno) .eqs. "Q" then exit
$ goto getver
$!
$getdst:
$ on warning then goto nodst
$ outdev = f$parse(dst,,,"device")
$ dst = f$parse(dst,"[F-O-O]",,"directory")
$ if dst .eqs. "[F-O-O]"
$  then
$ 	dst = outdev
$  else
$ 	dst = outdev + dst
$ endif
$ on warning then continue
$ return
$nodst:
$ ask dst "Destination"
$ if f$extract(0,1,dst) .eqs. "Q" then exit
$ goto getdst
