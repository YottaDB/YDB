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
	i update,'$$ALL^GDEVERIF d message^GDE(gdeerr("GDNOTSET"),"""""") q
	i update,'$$GDEPUT^GDEPUT  d message^GDE(gdeerr("GDNOTSET"),"""""") q
	d GDFIND,CREATE^GDEGET:create,LOAD^GDEGET:'create
	q
GDFIND	s file=$zparse(tfile,"",defgldext)
	i file="" s file=$ztrnlnm(tfile) s:file="" file=tfile d message^GDE(gdeerr("INVGBLDIR"),$zwrite(file)_":"_$zwrite(defgld)) s tfile=defgld
	s file=$zsearch($zparse(tfile,"",defgldext))
	i file="" s file=$zsearch($zparse(tfile,"",defgldext))
	i file="" s file=$zparse(tfile,"",defgldext),create=1 d message^GDE(gdeerr("GDUSEDEFS"),$zwrite(file))
	e  s create=0 d message^GDE(gdeerr("LOADGD"),$zwrite(file))
	q
