;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 2001 Sanchez Computer Associates, Inc.	;
;								;
; Copyright (c) 2023 YottaDB LLC and/or its subsidiaries.	;
; All rights reserved.						;
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
	i file="" do
	. ; .gld file does not exist. Check if .gldinprogress file exists. If so, we are in the small window in gdeput.m
	. ; (of a concurrent GDE edit invocation) where we have create .gldinprogress file but have not yet renamed it to
	. ; .gld. Wait for that to be done before moving on.
	. n tfile2,file2
	. s tfile2=tfile_"inprogress"
	. ; Total sleep time in the loop is 500 milliseconds (1 msec for each iteration).
	. ; If the .gldinprogress file exists but it has not been renamed in 500 msec, we give up on waiting
	. ; and move on with our ".gld" file creation.
	. f i=1:1:500 s file2=$zsearch($zparse(tfile2,"",defgldext)) q:file2=""  h 0.001
	. s file=$zsearch($zparse(tfile,"",defgldext))
	i file="" s file=$zparse(tfile,"",defgldext),create=1 d message^GDE(gdeerr("GDUSEDEFS"),$zwrite(file))
	e  s create=0 d message^GDE(gdeerr("LOADGD"),$zwrite(file))
	q
