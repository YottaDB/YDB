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
%OD	;GT.M %OD utility - octal to decimal conversion program
	;invoke at INT with %OD in octal to return %OD in decimal
	;invoke at FUNC as an extrinsic function
	;if you make heavy use of this routine, consider $ZCALL
	;
	s %OD=$$FUNC(%OD)
	q
INT	r !,"Octal: ",%OD s %OD=$$FUNC(%OD)
	q
FUNC(o)
	q:o<0 ""
	n d,dg
	s d=0,o=+o
	f dg=1:1:$l(o) s d=d*8+$e(o,dg)
	q d
