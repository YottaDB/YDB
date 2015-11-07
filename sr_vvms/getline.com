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
$!
$!      p1 - "image+offset"
$!
$ if (p1 .eqs. "")
$ then
$       write sys$output ""
$       write sys$output "Syntax :  @gtm$tools:getline image+offset"
$       write sys$output ""
$       write sys$output "      e.g. @gtm$tools:getline ""GTMSHR+0001FC"""
$       write sys$output ""
$       exit
$ endif
$!
$ define sys$output nl:
$ define sys$error  nl:
$!
$ savegtmgbldir   = f$trnlnm("gtm$gbldir","LNM$PROCESS_TABLE")
$ savegtmroutines = f$trnlnm("gtm$routines","LNM$PROCESS_TABLE")
$!
$ define /process gtm$gbldir   "gtm$map:mapdb.gld"
$ define /process gtm$routines "gtm$vrt:[pct]/nosrc,gtm$map/src=(gtm$map,gtm$src,gtm$vrt:[pct])"
$!
$ define /process getlineinput "''p1'"
$!
$ deassign sys$output
$ deassign sys$error
$!
$ curpriv=f$setprv("bypas")	! for writing in gtm$obj:*.lis and gtm$map:mapdb.dat
$ gtm
d ^mapoff($ztrnlnm("getlineinput"))
$!
$ curpriv=f$setprv(curpriv)
$!
$ define sys$output nl:
$ define sys$error  nl:
$!
$ deassign /process getlineinput
$!
$ if (savegtmgbldir .eqs. "")
$ then
$ 	deassign /process gtm$gbldir
$ else
$ 	define /process gtm$gbldir   "''savegtmgbldir'"
$ endif
$!
$ if (savegtmroutines .eqs. "")
$ then
$ 	deassign /process gtm$routines
$ else
$ 	define /process gtm$routines   "''savegtmroutines'"
$ endif
$!
$ deassign sys$output
$ deassign sys$error
$!
