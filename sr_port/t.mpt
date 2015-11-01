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
%T	;GT.M %T utility - write current time as [h]h:mm a/pm
	;invoke at INT to set %TIM to the current time as [h]h:mm a/pm
	;invoke at FUNC as an extrinsic special variable
	;supplied for illustration and compatibility
	;use of inline code is easy and efficient
	;
	w $$FUNC()
	q
INT	s %TIM=$$FUNC()
	q
FUNC()	n h
	s h=$h
	q +$zd(h,"12")_$zd(h,":60 AM")
