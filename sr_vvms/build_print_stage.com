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
$! build_print_stage.com - prints the current time and stage of the build with appropriate indentation
$!
$! p1 - string indicating the stage
$! p2 - string indicating "begin" or "end" or "middle" (they get 0 2 and 4 spaces indentation respectively)
$!
$  time_stamp = f$time()
$  if (p2 .eqs. "begin")  then write sys$output "    ''time_stamp' --> BEGIN ''p1'"
$  if (p2 .eqs. "middle") then write sys$output "    ''time_stamp' -->   ''p1'"
$! if (p2 .eqs. "end")    then write sys$output "    ''time_stamp' -->    END ''p1'"
