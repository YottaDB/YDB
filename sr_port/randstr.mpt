;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 2011 Fidelity Information Services, Inc	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%RANDSTR(strlen,charranges,patcodes,charset)	; random string generator
	;
	; This generates a random string of length strlen from an alphabet
	;  defined by charset or by charranges and patcodes.
	; strlen defaults to 8
	; charranges is defined in a series of ranges corresponding to for
	;  loop ranges, e.g., 48:2:57 to select the even decimal digits in
	;  ASCII or 48:1:57,65:1:70 to select the hexadecimal digits
	;  charranges defaults to 1:1:127 in both M and UTF-8 modes
	; patcodes is used to restrict the characters in the random string
	;  to those from charranges that match the pattern codes (use pattern
	;  code "E" for no restrictions); patcodes defaults to "AN"
	; if charset is pass-by-reference, the returned value can be used in
	;  future calls to save time rebuilding it, since this can be expensive.
	; if called as a function, returns random string
	; if called as a routine, writes random string on current output device
	;
	new i,n,z
	if '$length($get(charset)) do
	. new a,x,y		; if charset not provided, initialize it to alphabet from which to generate random string
	. xecute "for i="_$select($length($get(charranges)):charranges,1:"1:1:127")_" set x=$get(x)_$char(i)"
	. for i=$ascii("A"):1:$ascii("Z"),$ascii("a"):1:$ascii("z") set a=$get(a)_$char(i) ;	a has all legal pattern codes
	. ; default patcodes to "AN" if not specified; restrict to legal patcodes otherwise
	. set patcodes="1."_$select($length($get(patcodes)):$translate(patcodes,$translate(patcodes,a)),1:"AN")
	. for i=1:1:$length(x) set y=$extract(x,i) set:y?@patcodes charset=$get(charset)_y
	. quit
	set n=$length(charset)		   			     			; n has number of characters in alphabet
	for i=1:1:$select($length($get(strlen)):strlen,1:8) set z=$get(z)_$extract(charset,$random(n)+1) ; generate random string
	quit:$quit z	; extrinsic
	write z
	quit

