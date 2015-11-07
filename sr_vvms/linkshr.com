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
$! linkshr - link a shareable image
$! parameters:
$!	p1 = target directory
$!	p2 = object library directory
$!	p3 = link options
$!	P4 = link command suffix
$!
$ set def 'p1'
$ if p4 .nes. "" then $ p4 = ","+p4
$ alpha = (f$getsyi("arch_name") .eqs. "Alpha")
$ xtra :=
$ if .not. alpha then $ xtra := objlib/include=gtmvector,
$ define/user objlib 'p2'mumps.olb
$ define/user target 'p1'
$ gtmshrlink = "gtmshrlink." + f$element(alpha,",","vax,axp")
$ link 'p3' 'xtra' objlib/include=(cmerrors,cmierrors),gtm$tools:'gtmshrlink'/opt,sys$input/opt,target:secshrlink.opt/opt,target:release_name.opt/opt 'p4'
name = GTMSHR.EXE
