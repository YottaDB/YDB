;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
; Copyright (c) 1987-2019 Fidelity National Information		;
; Services, Inc. and/or its subsidiaries. All rights reserved.	;
;								;
; Copyright (c) 2018-2021 YottaDB LLC and/or its subsidiaries.	;
; All rights reserved.						;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%OD	;YottaDB %OD utility - octal to decimal conversion program
	;invoke at INT with %OD in octal to return %OD in decimal
	;invoke at FUNC as an extrinsic function
	;if you make heavy use of this routine, consider $ZCALL
	;
	set %OD=$$FUNC(%OD)
	quit
INT	read !,"Octal: ",%OD set %OD=$$FUNC(%OD)
	q
FUNC(o)
	quit:"-"=$zextract(o,1) ""
	quit:"-"=$extract(o) ""
	set:"+"=$extract(o) o=$extract(o,2,9999)
	new c,d,dg,l
	set d=0,l=$length(o)
	quit:(18<l) $$CONVERTBASE^%CONVBASEUTIL(o,8,10)
	for c=1:1:l set dg=$find("01234567",$extract(o,c)) quit:'dg  set d=(d*8)+(dg-2)
	quit d
