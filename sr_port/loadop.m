;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 2001, 2008 Fidelity Information Services, Inc	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
loadop	;load op codes
	n (opc,opx,opcdcnt,loadh)
	k opc,opx,opcdcnt
	s opcdcnt=0
	s file=loadh("opcode_def.h") o file:read u file
loop	r x i $zeof g fini
	i x?1"OPCODE_DEF"1.E s rec=x,opcdcnt=opcdcnt+1
	i  d proc
	g loop
fini	c file
	q
proc	s val=opcdcnt-1,cd=$p($p(rec,"(",2),",",1)
	s opc(cd)=val,opx(val)=cd
	q
err	u "" w rec,!,"error code=",ec,"   line=",opcdcnt,!
	u file
	q
