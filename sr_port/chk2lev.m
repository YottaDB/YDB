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
chk2lev	;check for op codes which are missing slots
	New x,i
	Set x=""
	For  Set x=$o(work(x)) Quit:x=""  Do
	. If x#1 Do  Quit
	. . Set x=x\1
	. . If "OC_CO"'=$Extract(opx(x),1,5) Do
	. . . For i=x+.1:.1:x+.3 If '$Data(work(i)) Write !,"subcode ",i-x*10," is missing for op code ",x," (",opx(x),")",!
	. . Set x=x+.9
	. If $Order(work(x))\1=x Write !,"triple code ",x," (",opx(x),")  is present with subcodes",!
	Quit
