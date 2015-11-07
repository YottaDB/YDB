;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
; Copyright (c) 2010-2015 Fidelity National Information 	;
; Services, Inc. and/or its subsidiaries. All rights reserved.	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

pinentry; Custom pinentry that returns an unobfuscated password if $gtm_passwd is defined in the environment
	;
	; See the following link for non-authoritative information on the pinentry protocol:
	; http://info2html.sourceforge.net/cgi-bin/info2html-demo/info2html/info2html?%28pinentry%29Protocol
	;
	set $etrap="do error",$zinterrupt=$etrap
	; gtm_passwd is validated as non-null by pinentry-gtm.sh
	set obfpwd=$zconvert($ztrnlnm("gtm_passwd"),"U"),obfpwdlen=$zlength(obfpwd)
	; Avoid NOBADCHAR on $PRINCIPAL - must use [io]chset on the OPEN and not USE
	open $principal:(ichset="M":ochset="M")
	; Unmask the password ahead of initiating the pinentry protocol. If the external call is not
	; available, the error handler is invoked
	set binobfpwd=""
	for i=1:2:$zlength(obfpwd) do
	. set msb=$zfind("0123456789ABCDEF",$zextract(obfpwd,i))-2
	. set lsb=$zfind("0123456789ABCDEF",$zextract(obfpwd,i+1))-2
	. set binobfpwd=binobfpwd_$zchar(16*msb+lsb)
	; If unmasking fails, exit
	do:$&gpgagent.unmaskpwd(binobfpwd,.clrpwds) error
	use $principal:(exception="goto error")
	write "OK Your orders please",!
	for  read in quit:'$zlength(in)  do
	. if "GETPIN"=$zconvert($zpiece(in," ",1),"U") write "D ",clrpwds,!,"OK",! zhalt 0
	. else  write "OK",!
	; Since this routine only responds to GETPIN, issue an error if it did not receive that command,
	; letting pinentry-gtm.sh execute the default pinentry
	zhalt 1

	; The error handler's primary function is to exit with error status so that the calling pinentry-gtm.sh
	; can execute the default pinentry program. If $gtm_pinentry_log is defined, the routine will dump all
	; status. Note that the locals are all killed prior to dumping status
error	kill
	set pinlog=$ztrnlnm("gtm_pinentry_log")
	new $etrap set $etrap="zhalt +$zstatus"
	if $zlength(pinlog) do
	. open pinlog:(append:chset="M")
	. use pinlog
	. write !,"PINENTRY-F-FAILED ",$zdate($horolog,"YYYY/MM/DD 24:60:SS"),!
	. zwrite $zversion,$ecode,$job,$zchset,$zdirectory,$zroutines,$zstatus
	. write "Stack trace:",! zshow "S"
	. write "Loaded external calls:",! zshow "C"
	. close pinlog
	zhalt +$zstatus
	quit
