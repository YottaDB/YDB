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
chkop	;display undefined triple codes
	u ""
	n x
	w !,"missing triple op codes: ",!
	s x=""
chkop1	s x=$o(opx(x)) i x="" g chkop9
	i '$d(work(x)),'$d(work(x+.1)) w x,?7,opx(x),!
	g chkop1
chkop9	q
