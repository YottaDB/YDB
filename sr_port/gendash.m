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
gendash	;generate sub-indices for dash type ttt's
	n (work,ttt,opcdcnt)
	f x=0:1:opcdcnt-1 d gd1
	q
gd1	i $d(work(x)) s ttt(x)=work(x) q
	i $o(work(x))\1'=x s ttt(x)=0 q
	s ttt(x)=ttt
	f i=x+.1:.1 d proc q:$o(work(i))\1'=x
	q
proc	i $d(work(i)) s ttt(ttt)=work(i)
	e  s ttt(ttt)=0
	s ttt=ttt+1
	q
