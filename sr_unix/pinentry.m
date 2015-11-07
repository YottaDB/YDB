;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 2010, 2014 Fidelity Information Services, Inc	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

pinentry	; Substitute pinentry that returns an unobfuscated password
		; if $gtm_passwd is defined in the environment.  If the command
		; received is not GETPIN, this runs /usr/bin/pinentry
		;
	set $etrap="write $zstatus,!,$stack($stack,""MCODE""),! halt"
	set obfpwd=$ztrnlnm("gtm_passwd"),obfpwdlen=$length(obfpwd)
	set obfpwd=$zconvert(obfpwd,"U")
	write "OK Your orders please",!
	set done=0
	for  quit:done  read in quit:'$length(in)  do
	. if "GETPIN"=$zconvert($piece(in," ",1),"U") do
	. . set obfpwds=""
	. . for i=1:2:$length(obfpwd) do
	. . . set msb=$find("0123456789ABCDEF",$extract(obfpwd,i))-2
	. . . set lsb=$find("0123456789ABCDEF",$extract(obfpwd,i+1))-2
	. . . set obfpwds=obfpwds_$zchar(16*msb+lsb)
	. . write:'$&gpgagent.unmaskpwd(obfpwds,.clrpwds) "D ",clrpwds,!
	. . set done=1
	. write "OK",!
	quit
