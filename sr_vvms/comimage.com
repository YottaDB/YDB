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
$! This file submits either PRO, BTA or DBG image for build
$! P1 is the target-version
$! P2 is the target-image (bta, dbg, pro)
$! P3 is the target-node (which determines the platform)
$! P4 is an optional quoted list of submit qualifiers (e.g. "/queue=alpha1_hiq/after=time")
$!
$ interact = (f$mode() .eqs. "INTERACTIVE")
$!
$ if p1 .eqs. ""
$  then
$   write sys$output "Must specify a version"
$   if interact then inquire p1 "Version"
$   if p1 .eqs. ""
$    then
$     write sys$output "No action taken"
$     exit
$   endif
$ endif
$!
$askimage:
$ if f$locate(p2,"/BTA/DBG/PRO") .eq. f$length("/BTA/DBG/PRO")
$  then
$   write sys$output "Must specify an image"
$   if interact
$    then
$     inquire p2 "Image (bta,dbg,pro)"
$     if f$locate(p2,"/BTA/DBG/PRO") .eq. f$length("/BTA/DBG/PRO")
$      then
$	write sys$output "Check Spelling"
$       goto askimage
$     endif
$    else
$     write sys$output "No action taken"
$     exit
$   endif
$ endif
$!
$ if p3 .eqs. "" then p3 = f$getsyi("nodename")
$!
$ if (p3 .eqs. "CETUS")
$  then
$   if interact
$    then
$	write sys$output " "
$	inquire link "Do you want to link the ''p2' files? Y/N	"
$	if ((link .eqs. "Y") .or. (link .eqs. "y"))
$	   then
$   		linkopt := YES
$          else
$		linkopt := NO
$	endif
$    else
$     linkopt := NO
$   endif
$  else
$   linkopt := YES
$ endif
$!
$ platform = f$getsyi("arch_name")
$ if (platform .eqs. "Alpha")
$  then
$    platform := AXP
$ endif
$!
$ @gtm$tools:build_print_stage "comimage ''p1' ''p2' on ''p3'" "begin"
$!
$ when = f$trnlnm("test_run_time")
$ if when .nes. "" then $ when = "/after=""" + when + """"
$ comlist = f$search("user:[library."+p1+"]"+platform+"_comlist.com",0)
$ if ( comlist .eqs. "" )
$ then
$   write sys$output "%COMIMAGE-E-NOCOMLIST, no comlist for version"
$   exit
$ endif
$ if (p2 .eqs. "PRO" .or. p2 .eqs. "P")
$    then
$       submit/noprint/name=buildpro/log=user:[library.'p1']buildpro.log -
	  /queue='p3'_hiq/parameters=("","",gtm$pro,'p1','linkopt') 'comlist''p4''when'
$	comimage_entry == $entry
$	exit
$ endif
$ if (p2 .eqs. "BTA" .or. p2 .eqs. "B")
$    then
$	submit/noprint/name=buildbta/log=user:[library.'p1']buildbta.log -
          /queue='p3'_hiq/parameters=("/debug","/nooptimize/debug",gtm$bta,'p1','linkopt') -
			'comlist''p4''when'
$	comimage_entry == $entry
$ 	exit
$ endif
$ if (p2 .eqs. "DBG" .or. p2 .eqs. "D")
$     then
$        submit/noprint/name=builddbg/log=user:[library.'p1']builddbg.log -
          /queue='p3'_hiq/parameters=("/debug","/nooptimize/debug/define=DEBUG",gtm$dbg,'p1','linkopt') -
			'comlist''p4''when'
$	comimage_entry == $entry
$ 	exit
$ endif
$ @gtm$tools:build_print_stage "comimage" "end"
$ exit
