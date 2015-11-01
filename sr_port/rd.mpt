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
%RD	;GT.M %RD utility - routines directory
	;invoke ^%RD for an interactive routine directory
	;invoke OBJ^%RSEL for an interactive directory based on object modules
	;
SRC
	n %ZE,%ZR
	s %ZE=".m"
	d RD^%RSEL
	q
OBJ
	n %ZE,%ZR
	s %ZE=$s($zver["VMS":".obj",1:".o")
	d RD^%RSEL
	q
LIB
	n %ZE,%ZR
	s %ZE=".m",%ZR="%*"
	d RD^%RSEL
	q
