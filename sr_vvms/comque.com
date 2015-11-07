$!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
$!								!
$!	Copyright 2001, 2005 Fidelity Information Services, Inc	!
$!								!
$!	This source code contains the intellectual property	!
$!	of its copyright holder(s), and is made available	!
$!	under a license.  If you do not know the terms of	!
$!	the license, please stop and do not read further.	!
$!								!
$!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
$!
$! p1 is the version to build
$! p2 is the node on which to build; defaults to the node on which it is submitted
$! p3 can be used to specify a quoted list of submit qualifiers like "/hold" or "/after=0-23:59"
$!
$ interact = (f$mode() .eqs. "INTERACTIVE")
$!
$ if p1 .eqs. "" then p1 = f$trnlnm("gtm$verno")
$ if p2 .eqs. "" then p2 = f$getsyi("nodename")
$!
$ @gtm$com:setactive 'p1' p
$ @gtm$tools:comimage 'p1' "PRO" 'p2' 'p3'
$ comimage_pro_entry = comimage_entry
$
$ @gtm$tools:comimage 'p1' "DBG" 'p2' 'p3'
$ comimage_dbg_entry = comimage_entry
$
$ @gtm$tools:buildhlp 'p1'
$
$ set entry /release 'comimage_pro_entry'
$ sync /entry='comimage_pro_entry'
$ set noverify
$ search gtm$ver:buildpro.log "-W-","-E-","-F-" /out=gtm$ver:errorpro.log
$ comimage_pro_status = $status
$ set verify
$ if (comimage_pro_status .eqs. "%X00000001") then goto error_handler
$ if (((comimage_pro_status / 2) * 2) .eqs. comimage_pro_status) then goto error_handler
$ delete/log gtm$ver:errorpro.log.
$
$ set entry /release 'comimage_dbg_entry'
$ sync /entry='comimage_dbg_entry'
$ set noverify
$ search gtm$ver:builddbg.log "-W-","-E-","-F-" /out=gtm$ver:errordbg.log
$ comimage_dbg_status = $status
$ set verify
$ if (comimage_dbg_status .eqs. "%X00000001") then goto error_handler
$ if (((comimage_dbg_status / 2) * 2) .eqs. comimage_dbg_status) then goto error_handler
$ delete/log gtm$ver:errordbg.log.
$
$ exit
$
$error_handler:
$ exit
