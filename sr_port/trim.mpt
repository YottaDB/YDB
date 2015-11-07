;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 2012, 2014 Fidelity Information Services, Inc	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%TRIM	; routine to strip of leading and trailing characters; defaulting to spaces and tabs
	for  read line quit:$zeof  write $$FUNC(line),!
	quit

FUNC(s,ch)
	set:'$length($get(ch)) ch=$char(9,32)
	quit $$L($$R(s,ch),ch)

L(s,ch)	new i,len,tmp
	set:'$length($get(ch)) ch=$char(9,32)
	set len=$length(s)
	for i=1:1:len+1 quit:ch'[$extract(s,i)
	quit $extract(s,i,len)

R(s,ch)	new i,tmp
	set:'$length($get(ch)) ch=$c(9,32)
	for i=$length(s):-1:0 quit:ch'[$extract(s,i)
	quit $extract(s,1,i)
