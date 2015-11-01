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
%HO	;GT.M %HO utility - hexadecimal to octal conversion program
	;invoke at INT with %HO in hexadecimal to return %HO in octal
	;invoke at FUNC as an extrinsic function
	;if you make heavy use of this routine, consider $ZCALL
	;
	s %HO=$$FUNC(%HO)
	q
INT	r !,"Hexadecimal: ",%HO s %HO=$$FUNC(%HO)
	q
FUNC(h)
	q:$tr(h,"E","e")<0 ""
	n c,d,dg,o
	s d=0,h=$tr(h,"abcdef","ABCDEF"),o=""
	f c=1:1:$l(h) s dg=$f("0123456789ABCDEF",$e(h,c)) q:'dg  s d=(d*16)+(dg-2)
	f  q:'d  s o=d#8_o,d=d\8
	q:0<o o
	q 0
	;
