;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 1989,2001 Sanchez Computer Associates, Inc.	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%EXP	;GT.M %EXP utility - raise 1st argument to the power of the 2nd
	;invoke with %I and %J (%I**%J) to obtain result in %I
	;invoke at INT to execute interactively
	;invoke at FUNC as an extrinsic function
	;
	s %I=$$FUNC(%I,%J)
	q
INT	n %I,%J
	r !,"Power:  ",%J r !,"Number: ",%I w !,%I," raised to ",%J," is ",$$FUNC(%I,%J),!
	q
FUNC(i,j)
        n f,w
	i i<0,j#1 q ""
	q i**j
