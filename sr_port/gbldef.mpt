;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 2001, 2013 Fidelity Information Services, Inc	;
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
	i $tl zm 150383202	; TPNOSUPPORT error
	i '$$edit(.gname) q 0
	i $zfind($view("REGION",gname),",") zm 150383194:gname	; ISSPANGBL
	i "BGMM"'[$view("GVACCESS_METHOD",$view("REGION",gname)) zm 150376418:$view("REGION",gname) ; DBREMOTE
	i $d(@gname) zm 150373626	; Error if there is data in the global
	s @gname="" k @gname		; make sure that the global is defined
	s gname=$zextract(gname,2,32)		; remove circumflex, take at most 31 chars
	v "YDIRTVAL":$zextract($view("YDIRTREE",gname),1,4),"YDIRTREE":gname
	q 1
	;
set(gname,nct,act)
	n ver,$et
	s $et="g error"
	i $tl zm 150383202	; TPNOSUPPORT error
	i '$$edit(.gname) q 0
	i $zfind($view("REGION",gname),",") zm 150383194:gname	; ISSPANGBL
	i "BGMM"'[$view("GVACCESS_METHOD",$view("REGION",gname)) zm 150376418:$view("REGION",gname) ; DBREMOTE
	i $d(@gname) zm 150373626 		; Error if there is data in the global
	s act=+$g(act),nct=+$g(nct) s:nct nct=1
	i (act>255)!(act<0) zm 150374290:act	; collation type specified is illegal
	s ver=$view("YCOLLATE",act)
	i ver<0 zm 150376282:act		; doesn't find coll type, or can't get version
	s @gname="" k @gname			; make sure that the global is defined
	s gname=$zextract(gname,2,32)			; remove circumflex, take at most 31 chars
	v "YDIRTVAL":$zextract($view("YDIRTREE",gname),1,4)_$zchar(1,nct,act,ver),"YDIRTREE":gname
	q 1
	;
get(gname,reg)
	n t,tl,$et,nct,act,ver,ret,dir,error
	s $et="g error"
	i '$$edit(.gname) q 0
	s gname=$zextract(gname,2,32)
	i '$d(reg) s reg=$piece($view("REGION","^"_gname),",",1)
	i "BGMM"'[$view("GVACCESS_METHOD",reg) zm 150376418:reg ; DBREMOTE
	; -----------------------------------
	; first check in directory tree
	s dir=$view("YDIRTREE",gname,reg)
	s t=$zextract(dir,5,999),tl=$zl(t)	; remove circumflex, take at most 31 chars
	i tl,tl>4!($a(t,1)'=1) zm 150374058
	i tl s nct=$a(t,2),act=$a(t,3),ver=$a(t,4) q nct_","_act_","_ver
	; -----------------------------------
	; db directory tree has no collation information. next check for gld.
	s t=$view("YGLDCOLL","^"_gname)
	i t'="0" s nct=$piece(t,",",1),act=$piece(t,",",2),ver=$piece(t,",",3)  q nct_","_act_","_ver
	; -----------------------------------
	; no collation information was found in gld. check for coll info from db file hdr
	s nct=0 d  q:(act=0) 0
	. d getzpeek("FHREG:"_reg,64,4,"U",.act)
	. d getzpeek("FHREG:"_reg,68,4,"U",.ver)
	q nct_","_act_","_ver
	;
getzpeek(mnemonic,offset,length,format,result)
	n xstr
	i $zver'["VMS" s xstr="s result=$zpeek(mnemonic,offset,length,format)" x xstr  q
	s result=0
	q
edit(gname)
	i $zextract(gname)'="^" s gname="^"_gname
	i $zextract(gname,2)'="%",$zextract(gname,2)'?1A zm 150373218		; LKNAMEXPECTED
	i gname'?1"^"1E.AN zm 150373218
	q 1
	;
error	s $ec="",error=1
	q 0
