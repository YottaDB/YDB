;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
; Copyright (c) 2018-2019 Fidelity National Information		;
; Services, Inc. and/or its subsidiaries. All rights reserved.	;
;								;
; Copyright (c) 2018-2021 YottaDB LLC and/or its subsidiaries.	;
; All rights reserved.						;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
ADDVAL(v,a1,a2,b)	;Computes val1+val2 for any arbitrary-length val1 and val2 and for base <= 16
	; v - transform array as set up by CONVERTBASE
	; a1, a2 - addends; b - base of addends
	new c,i,l,l1,l2,s,t,t1,t2
	; c =carry; i - iterator; l - greater length of l1 & l2 - addend lengths; s - sum; t1 & t2 - temporaries
	set c=0,s="",t1=a1,t2=a2,l1=$length(t1),l2=$length(t2),l=$select(l2>l1:l2,1:l1)
	set:l1<l $p(s,0,l-l1+1)="",t1=s_t1,s=""
	set:l2<l $p(s,0,l-l2+1)="",t2=s_t2,s=""
	for i=l:-1:1 set t=v($extract(t1,i))+v($extract(t2,i))+c,s=v(t#b)_s,c=t\b
	quit $select(c:c_s,1:s)

MULTIPLYBYNUMBER(v,m,x,b)	; for base<=10, multiplies 2 arbitrary-length numbers, else only 1 digit numbers
	; v - transform array as set up by CONVERTBASE
	; m - multipicand; x - multiplier; b - base
	new r			; computes m*x using x*((m>>2^0)&&1)*2^0 + ((m>>2^1)&&1)*2^1 + ((m>>2^2)&&1)*2^2 +...)
	set r=0			; r - result
	set:$find("ABCDEF",m) m=v(m)
	for  quit:'m  set:m#2 r=$$ADDVAL(.v,r,x,b) set m=m\2,x=$$ADDVAL(.v,x,x,b)
	quit r

CONVERTBASE(val,fb,tb)	; converts val from base fb to base tb
	new e,i,o,r,v	; e - exponent; i - iterator; o - offset r - result ; v - transform array used by subroutines
	for i=0:1:15 set:i<10 v(i)=i set:9<i v(i)=$char(i+55),v($char(i+55))=i
	set r=0,e=1,o=$$VALIDLEN(val,fb)
	quit:0=o
	for o=o:-1:1 set r=$$ADDVAL(.v,r,$$MULTIPLYBYNUMBER(.v,$extract(val,o),e,tb),tb),e=$$MULTIPLYBYNUMBER(.v,fb,e,tb)
	quit r
CONVNEG(v,base)	;Takes a base 16 or base 8 number and converts it into its two's compliment representation (flip bits and add one)
	new i,c,n,charmap,pocharmap	; c - complement; i - iterator; n - negated result
	if 8=base set charmap="01234567"
	else  quit:16'=base "" set charmap="0123456789ABCDEF"
	set pocharmap=($extract(charmap,2,$length(charmap)))_"0"
	set v=$translate(v,charmap,$reverse(charmap))
	set n="",c="0"
	for i=0:1:$length(v)  quit:(c'="0")  set c=$extract(v,$length(v)-i),c=$translate(c,charmap,pocharmap),n=c_n
	set:i'=$length(v) n=$extract(v,1,$length(v)-i)_n
	set:$find(($extract(charmap,1,(base/2))),$extract(n,1)) n=($extract(charmap,$length(charmap)))_n
	quit n
VALIDLEN(val,base) ;Returns length of string until which all characters are valid for the given base (works up to base 16)
	new i,valbasechar,invalidi,len
	set valbasechar=$extract("0123456789ABCDEF",1,base),len=$length(val)
	quit:'len 0
	for i=1:1:$length(val)  set invalidi='($find(valbasechar,($extract(val,i))))  quit:invalidi
	quit $select($get(invalidi):i-1,1:i)
