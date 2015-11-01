;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 1987, 2003 Sanchez Computer Associates, Inc.	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%DH	;GT.M %DH utility - decimal to hexadecimal conversion program
	;invoke with %DH in decimal and %DL digits to return %DH in hexadecimal
	;invoke at INT to execute interactively
	;invoke at FUNC as an extrinsic function
	;if you make heavy use of this routine, consider $ZCALL
	;
	s %DH=$$FUNC(%DH,$G(%DL))
	q
INT	n %DL
	r !,"Decimal: ",%DH r !,"Digits:  ",%DL s %DH=$$FUNC(%DH,%DL)
	q
FUNC(d,l)
	s:'$l($g(l)) l=8
	q:d=0 $e("00000000",1,l)
	n h
	s h=""
	s:d<0 d=$$FUNC^%EXP(16,l)+d
	f  q:'d  s h=$e("0123456789ABCDEF",d#16+1)_h,d=d\16
	q $e("00000000",1,l-$l(h))_h
