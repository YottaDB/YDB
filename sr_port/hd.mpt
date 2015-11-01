;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 1987,2001 Sanchez Computer Associates, Inc.	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%HD	;GT.M %HD utility - hexadecimal to decimal conversion program
	;invoke at INT with %HD in hexadecimal to return %HD in decimal
	;invoke at FUNC as an extrinsic function
	;if you make heavy use of this routine, consider $ZCALL
	;
	s %HD=$$FUNC(%HD)
	q
INT	r !,"Hexidecimal: ",%HD s %HD=$$FUNC(%HD)
	q
FUNC(h)
	q:$tr(h,"E","e")<0 ""
	n c,d,dg
	s d=0,h=$tr(h,"abcdef","ABCDEF")
	f c=1:1:$l(h) s dg=$f("0123456789ABCDEF",$e(h,c)) q:'dg  s d=(d*16)+(dg-2)
	q d
