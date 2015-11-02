;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%gbldef	; ; ;Global Collation Control
	;
kill(gname)
	n $et
	s $et="g error"
	i '$$edit(.gname) q 0
	i "BGMM"'[$v("GVACCESS_METHOD",$v("REGION",gname)) zm 150376418:$v("REGION",gname); DBREMOTE
	i $d(@gname) zm 150373626	;Error if there is data in the global
	s @gname="" k @gname		;make sure that the global is defined
	s gname=$e(gname,2,32)		;remove circumflex, take at most 31 chars
	v "YDIRTVAL":$e($v("YDIRTREE",gname),1,4),"YDIRTREE":gname
	q 1
	;
set(gname,nct,act)
	n ver,$et
	s $et="g error"
	i '$$edit(.gname) q 0
	i "BGMM"'[$v("GVACCESS_METHOD",$v("REGION",gname)) zm 150376418:$v("REGION",gname); DBREMOTE
	i $d(@gname) zm 150373626 		;Error if there is data in the global
	s act=+$g(act),nct=+$g(nct) s:nct nct=1
	i (act>255)!(act<0) zm 150374290:act	; collation type specified is illegal
	i act s ver=$V("YCOLLATE",act)
	e  s ver=0
	i ver<0 zm 150376282:act		; doesn't find coll type, or can't get version
	s @gname="" k @gname			;make sure that the global is defined
	s gname=$e(gname,2,32)			;remove circumflex, take at most 31 chars
	v "YDIRTVAL":$e($v("YDIRTREE",gname),1,4)_$c(1,nct,act,ver),"YDIRTREE":gname
	q 1
	;
get(gname)
	n t,tl,$et
	s $et="g error"
	i '$$edit(.gname) q 0
	i "BGMM"'[$v("GVACCESS_METHOD",$v("REGION",gname)) zm 150376418:$v("REGION",gname); DBREMOTE
	s t=$e($v("YDIRTREE",$e(gname,2,32)),5,999),tl=$l(t)	;remove circumflex, take at most 31 chars
	i tl,tl>4!($a(t,1)'=1) zm 150374058
	q $s(tl:$a(t,2)_","_$a(t,3)_","_$a(t,4),1:0)
	;
edit(gname)
	i $e(gname)'="^" s gname="^"_gname
	i $e(gname,2)'="%",$e(gname,2)'?1A zm 150373218		; LKNAMEXPECTED
	i gname'?1"^"1E.AN zm 150373218
	q 1
	;
error	s $ec=""
	q 0
