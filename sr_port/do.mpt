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
%DO	;GT.M %DO utility - decimal to octal conversion program
	;invoke with %DO in decimal and %DL digits to return %DO in octal
	;invoke at INT to execute interactively
	;invoke at FUNC as an extrinsic function
	;if you make heavy use of this routine, consider $ZCALL
	;
	s %DO=$$FUNC(%DO,$g(%DL))
	q
INT	n %DL
	r !,"Decimal: ",%DO r !,"Digits:  ",%DL s %DO=$$FUNC(%DO,%DL)
	q
FUNC(d,l)
	s:'$l($g(l)) l=12
	q:d=0 $e("000000000000",1,l)
	n o
	s o=""
	s:d<0 d=$$FUNC^%EXP(8,l)+d
	f  q:'d  s o=d#8_o,d=d\8
	q $e("000000000000",1,l-$l(o))_o
