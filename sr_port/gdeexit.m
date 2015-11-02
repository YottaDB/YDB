;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 2001 Sanchez Computer Associates, Inc.	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
exit:	;implement the verb: EXIT
EXIT
	i 'update d QUIT^GDEQUIT
	i '$$ALL^GDEVERIF  zm gdeerr("NOEXIT")  q
	i '$$GDEPUT^GDEPUT  q	; zm is issued in GDEPUT.m
	h
