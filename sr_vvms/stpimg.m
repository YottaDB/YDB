;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 1991, 2002 Sanchez Computer Associates, Inc.	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
stpimg	; stpimg is takes parameters passed to GT.M by the invoking CLI and
	; passes them to ^PID which tries to stop all processes running a
	; given image.
	; P1 is the image name
	; P2 is the number of second to wait after issuing a FORCEX before
	;  using a DELPRC against a process which doesn't leave the image
	;
	n image,$zt
	s image=$p($zcmdline," ")
	i '$l(image) w "Invoked with no image name" q
	d STPIMG^PID(image,+$p($zcmdline," ",2))
	q
