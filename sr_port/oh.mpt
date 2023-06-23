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
%OH	;GT.M %OH utility - octal to hexadecimal conversion program
	;invoke at INT with %OH in octal to return %OH in hexadecimal
	;invoke at FUNC as an extrinsic function
	;if you make heavy use of this routine, consider $ZCALL
	;
	set %OH=$$FUNC(%OH)
	quit
INT	read !,"Octal: ",%OH set %OH=$$FUNC(%OH)
	write:%OH="" !,"Input must be an octal number"
	quit
FUNC(o)
	quit:"0"[$get(o) 0
	quit:"-"=$extract(o) ""
	new c,d,dg,h,l
	set d=0,h="",l=$length(o)
	quit:(18<l) $$CONVERTBASE^%CONVBASEUTIL(o,8,16)
	for c=1:1:l set dg=$find("01234567",$extract(o,c)) quit:'dg  set d=(d*8)+(dg-2)
	for  quit:'d  set h=$e("0123456789ABCDEF",d#16+1)_h,d=d\16
	quit h
