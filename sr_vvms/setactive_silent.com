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
$! same as setactive.com except that this does not print output unless "set verify" is on
$!
$ if f$environment("VERIFY_PROCEDURE")
$ then
$	ver 'p1' 'p2'
$ else
$	define sys$output nl:
$	define sys$error  nl:
$	ver 'p1' 'p2'
$	deassign sys$output
$	deassign sys$error
$ endif
$!
