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
$! cre_comlistxp invokes commalxp to create a typ_comlist.com compile driver
$! 	--- assumes "version" command was executed before coming in here
$!
$ set def gtm$ver
$ curver = f$element(1,".",f$trnlnm("gtm$vrt"))
$ curimg = f$extract(0,1,f$element(1,"$",f$trnlnm("gtm$exe")))
$ define gtm$libsrc axp_gtm$gtmdev:[library.'curver'.src]
$ show log gtm$libsrc
$! "expand" gtm$src, i.e. get all directories in the search list, if
$! it is a search list
$ src1 = f$trnlnm("gtm$src",,0)
$ src2 = f$trnlnm("gtm$src",,1)
$ define gtmsrc "''src1'"
$ if "" .nes. src2 then define/nolog gtmsrc "''src1',''src2'"
$ define gtm$routines "[]/src=(''f$trnlnm("gtmsrc")')"
$ version p p
$ gtm
o "SYS$COMMAND" u "SYS$COMMAND" d noprompt^comall
$ version 'curver' 'curimg'
$ delete comall.obj.
$ exit
