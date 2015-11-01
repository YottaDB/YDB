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
%HEX2UTF;GT.M %HEX2UTF utility - hexadecimal byte stream to UTF-8 string
	;invoke with %U storing the hexadecimal byte stream to return %S having the converted string
	;invoke at INT to execute interactively
	;invoke at FUNC as an extrinsic function
	;

	set %S=$$FUNC(%U)
	quit

INT	new %U
	read !,"Hexadecimal byte stream: ",%U set %S=$$FUNC(%U)
	quit

FUNC(u)
	new l
	set l=$ZLENGTH(u)
	quit $$recurse(1,l)

recurse(start,end);
	new l,m
	set l=end-start+1
	if (l<512) quit $$eval(start,end)
	set m=l\2
	if (m#2=1) set m=m+1
	quit $$recurse(start,start+m-1)_$$recurse(start+m,end)

eval(start,end);
	new i,s,d,s1
	set s1="$zchar("
	for i=start:2:end-2 set s=$zextract(u,i,i+1),d=$$FUNC^%HD(s),s1=s1_d_","
	set i=end-1,s=$zextract(u,i,i+1),d=$$FUNC^%HD(s),s1=s1_d
	set s1=s1_")"
	quit @s1
