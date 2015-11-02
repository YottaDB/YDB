;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 2001, 2012 Fidelity Information Services, Inc	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%lclcol	; ; ; Local Variable Collation Control
	;
get()		q $v("YLCT")
getncol()	q $v("YLCT","ncol") ; return null collation order
	;
getnct()	q $v("YLCT","nct") ; return numeric collation type
	;
set(lct,ncol,nct)
	n ok,$et
	s $et="s $ec="""" s ok=0",ok=1
	s:'$data(lct) lct=-1
	s:'$data(ncol) ncol=-1
	s:'$data(nct) nct=-1
	v:ok "YLCT":lct:ncol:nct
	q ok
