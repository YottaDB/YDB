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
$! buildcm - build gtcm_server, cmishr and gtcm_stop
$! parameters:
$!	p1 = version number
$!	p2 = library (p, d, or b)
$!	p3 = target device and directory
$!
$ @gtm$tools:buildaux 'p1' 'p2' 'p3' "cmi"
$ @gtm$tools:buildaux 'p1' 'p2' 'p3' "gtcm_server"
$ @gtm$tools:buildaux 'p1' 'p2' 'p3' "gtcm_stop"
