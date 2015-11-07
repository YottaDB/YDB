;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;									;
;	Copyright 2001, 2014 Fidelity Information Services, Inc         ;
;									;
;	This source code contains the intellectual property		;
;	of its copyright holder(s), and is made available		;
;	under a license.  If you do not know the terms of		;
;	the license, please stop and do not read further.		;
;									;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%lclcol	; ; ; Local Variable Collation Control
	;
get()		quit $view("YLCT")
getncol()	quit $view("YLCT","ncol") ; return null collation order
	;
getnct()	quit $view("YLCT","nct") ; return numeric collation type
	;
set(lct,ncol,nct)
	new $etrap
	set $etrap="set $ecode="""" quit 0"
	set:""=$get(lct) lct=$view("YLCT")
	set:""=$get(ncol) ncol=$view("YLCT","ncol")
	set:""=$get(nct) nct=$view("YLCT","nct")
	if (lct'=$view("YLCT"))!(ncol'=$view("YLCT","ncol"))!(nct'=$view("YLCT","nct")) view "YLCT":lct:ncol:nct
	quit 1
