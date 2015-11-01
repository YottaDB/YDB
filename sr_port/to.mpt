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
%TO	;GTM %TO utility - return current time in %TS as [h]h:mm a/pm
	;invoke with %TN in $H format to set %TS to [h]h:mm a/pm
	;supplied for illustration and compatibility
	;use of inline code is easy and efficient
	;
INT	n tn s tn=$g(%TN) s:'$D(%TN) tn=$p($H,",",2) s %TS=+$zd(","_tn,"12")_$zd(","_tn,":60 AM")
	q
