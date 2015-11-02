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
%D	;GT.M %D utility - write current date as [d]d-mmm-yy
	;invoke at INT to set %DAT to the current date as [d]d-mmm-yy
	;invoke at FUNC as an extrinsic special variable
	;supplied for illustration and compatibility
	;use of inline code is easy and efficient
	;
	w $$FUNC()
	q
INT	s %DAT=$$FUNC()
	q
FUNC()	n h,monyear
	s monyear=$S($ZDATEFORM:"-MON-YEAR",1:"-MON-YY")
	s h=$h
	q +$zd(h,"DD")_$zd(h,monyear)
