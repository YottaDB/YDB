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
log:	;implement the verb: LOG
LOG
	i gqual="OFF" s log=0 d message^GDE(gdeerr("LOGOFF"),$zwrite(logfile)) q
	s log=1
	i $d(gqual("value")) s logfile=$zparse(gqual("value"),"","",".LOG")
	o logfile:(newversion:noreadonly) d message^GDE(gdeerr("LOGON"),$zwrite(logfile))
	q
INQUIRE
	i 'log d message^GDE(gdeerr("NOLOG"),$zwrite(logfile))
	e  d message^GDE(gdeerr("LOGON"),$zwrite(logfile))
	q
