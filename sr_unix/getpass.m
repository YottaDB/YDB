;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 2009 Fidelity Information Services, Inc	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

getpass(passlen)
	new isave,zsd,i,pass
	if $IO'=$P set isave=$IO use $P
	zshow "D":zsd
	set i=""
	for  set i=$order(zsd("D",i)) q:i=""  if $P=$piece(zsd("D",i)," ",1) set zsd=$find(zsd("D",i),"NOECHO",$length($p))
	use $P:(NOECHO)
	read !,"Enter Passphrase: ",pass#passlen,!
	if 'zsd use $P:ECHO
	if $d(isave) use isave
	quit pass
