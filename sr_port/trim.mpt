;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 2012 Fidelity Information Services, Inc.    	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%TRIM	; routine to strip of leading and trailing spaces/tabs
	for  quit:$zeof  read line write !,$$FUNC(line),!
	quit

FUNC(s) quit $$L($$R(s))

L(s)	new i,l,tmp
	set l=$length(s)
	for  set tmp=$extract(s,$increment(i)) quit:" "'=tmp&($c(9)'=tmp)!'$length(tmp)
	quit $extract(s,i,$length(s))

R(s)	new i,l,tmp
	set i=$length(s)+1 for  set tmp=$extract(s,$increment(i,-1)) quit:" "'=tmp&($c(9)'=tmp)!'i
	quit $extract(s,1,i)
