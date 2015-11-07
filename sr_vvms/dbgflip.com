$!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
$!								!
$!	Copyright 2003, 2008 Fidelity Information Services, Inc	!
$!								!
$!	This source code contains the intellectual property	!
$!	of its copyright holder(s), and is made available	!
$!	under a license.  If you do not know the terms of	!
$!	the license, please stop and do not read further.	!
$!								!
$!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
$!
$! dbgflip.com - turn "off" or "on" debug bit in gtm$dbg:*.exe and gtm$bta.exe (if it exists). Default is to turn "off"
$!
$ if p1 .eqs. ""
$ then
$	off_or_on = "off"
$ else
$	off_or_on = "''p1'"
$ endif
$!
$ nodebugi := $gtm$bin:flipdebug.exe
$!
$bta_loop:
$       exename = f$search("gtm$bta:*.exe")
$       if (exename .eqs. "") then goto end_bta_loop
$	write sys$output "nodebugi ''exename' ''off_or_on' alpha"
$	nodebugi 'exename' 'off_or_on' alpha
$       goto bta_loop
$end_bta_loop:
$
$dbg_loop:
$       exename = f$search("gtm$dbg:*.exe")
$       if (exename .eqs. "") then goto end_dbg_loop
$	write sys$output "nodebugi ''exename' ''off_or_on' alpha"
$	nodebugi 'exename' 'off_or_on' alpha
$       goto dbg_loop
$end_dbg_loop:
$
