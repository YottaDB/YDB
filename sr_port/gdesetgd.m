;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 2001 Sanchez Computer Associates, Inc.	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
gdesetgd:	;implement the verb: SETGD
GDESETGD
	i update,'$$ALL^GDEVERIF zm gdeerr("GDNOTSET") q
	i update,'$$GDEPUT^GDEPUT  zm gdeerr("GDNOTSET") q
	d GDFIND,CREATE^GDEGET:create,LOAD^GDEGET:'create
	q
GDFIND	s file=$zparse(tfile,"",defgldext)
	i file="" s file=$ztrnlnm(tfile) s:file="" file=tfile zm gdeerr("INVGBLDIR"):file:defgld s tfile=defgld
	s file=$zsearch($zparse(tfile,"",defgldext))
	i file="" s file=$zsearch($zparse(tfile,"",defgldext))
	i file="" s file=$zparse(tfile,"",defgldext),create=1 zm gdeerr("GDUSEDEFS"):file
	e  s create=0 zm gdeerr("LOADGD"):file
	q
