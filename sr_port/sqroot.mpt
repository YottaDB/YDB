;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 1988,2001 Sanchez Computer Associates, Inc.	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%SQROOT	;GT.M %SQROOT utility - square root function
	;invoke at INT for interactive output to current device
	;invoke ^%SQROOT with %X to return the square root %Y
	;invoke at FUNC as an extrinsic function
	;
	s %Y=$$FUNC($g(%X))
	q
INT	n %X,%Y
	f  r !,"The square root of: ",%X q:+%X'=%X  w ?35," is: ",$$FUNC(%X),!
	q
FUNC(x)
	q:+x=0 0
	n i s i="" s:x<0 x=-x,i="i"
	q i_(x**.5)
