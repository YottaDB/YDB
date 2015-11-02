;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 2010 Fidelity Information Services, Inc	;
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
	Set $ETrap="Write $ZStatus,!,$Stack($Stack,""MCODE""),! Halt"
	Set obfpwd=$ZTRNLNM("gtm_passwd"),obfpwdlen=$Length(obfpwd)
	Write "OK Your orders please",!
	Set done=0
	For  Quit:done  Read in Quit:'$Length(in)  Do
	. If "GETPIN"=$Translate($Piece(in," ",1),"abcdefghijklmnopqrstuvwxyz","ABCDEFGHIJKLMNOPQRSTUVWXYZ") Do
	. . Set obfpwds=""
	. . For i=1:2:$Length(obfpwd) Set obfpwds=obfpwds_$ZCHar(16*($Find("0123456789ABCDEF",$Extract(obfpwd,i))-2)+$Find("0123456789ABCDEF",$Extract(obfpwd,i+1))-2)
	. . Write:'$&gpgagent.unmaskpwd(obfpwds,.clrpwds,$Length(obfpwds)) "D ",clrpwds,!
	. . Set done=1
	. Write "OK",!
	Quit
