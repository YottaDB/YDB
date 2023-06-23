;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 2006 Fidelity Information Services, Inc	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%UTF2HEX;GT.M %UTF2HEX utility - UTF-8 string to hexadecimal byte notation
	;invoke with %S storing the UTF-8 string to return %U having the hexadecimal byte notation
	;invoke at INT to execute interactively
	;invoke at FUNC as an extrinsic function
	;

	set %U=$$FUNC(%S)
	quit

INT	new %S
	read !,"String: ",%S set %U=$$FUNC(%S)
	quit

FUNC(s)
	new l
	set l=$ZLENGTH(s)
	quit $$recurse(1,l)

recurse(start,end);
	new l,m
	set l=end-start+1
	if (l<1024) quit $$eval(start,end)
	set m=l\2
	quit $$recurse(start,start+m-1)_$$recurse(start+m,end)

eval(start,end);
	new u,a,i
	set u=""
	for i=start:1:end set a=$ZASCII(s,i),u=u_$$FUNC^%DH(a,2)
	quit u
