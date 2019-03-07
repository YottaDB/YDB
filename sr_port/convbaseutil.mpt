;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
; Copyright (c) 2018 Fidelity National Information		;
; Services, Inc. and/or its subsidiaries. All rights reserved.	;
;								;
; Copyright (c) 2018 YottaDB LLC and/or its subsidiaries.	;
; All rights reserved.						;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;Computes val1+val2 for any arbitrary-length val1 and val2 and for  base <= 16
ADDVAL(val1,val2,base)
	new carry,res,len,currsum,len1,len2,val1r,val2r,tmp1,tmp2,i,c,d,j,k
	for k=0:1:16 set:k<10 c($CHAR(k+48))=k SET:k'<10 c($CHAR(k+55))=k
	for j=0:1:16 set:j<10 d(j)=$CHAR(j+48) SET:j'<10 d(j)=$CHAR(j+55)
	set val1r=$reverse(val1),val2r=$reverse(val2)
	set carry=0,res="",len1=$length(val1r),len2=$length(val2r),len=len1
	set:len2>len1 len=len2
	for i=1:1:len do
	. set tmp1=0,tmp2=0
	. set:(i'>len1) tmp1=c(($extract(val1r,i))) set:(i'>len2) tmp2=c(($extract(val2r,i)))
	. set currsum=tmp1+tmp2+carry
	. set res=d((currsum#base))_res,carry=$piece((currsum/base),".")
	set:carry>0 res=carry_res
	quit res
;Computes num*x by using x*((num>>2^0)&&1)*2^0 + ((num>>2^1)&&1)*2^1 + ((num>>2^2)&&1)*2^2 +...)
;Will multiply two arbitrary-length numbers for base <= 10, but for base > 10 will only multiply
;single digit numbers
MULTIPLYBYNUMBER(num,x,base)
	new res,k,c
	if ($find("ABCDEF",num)) do
	. for k=0:1:16 set:k<10 c($CHAR(k+48))=k SET:k'<10 c($CHAR(k+55))=k
	. set num=c(num)
	set res="0"
	for  quit:num=0  do
	. if (num#2>0) do
	.. set res=$$ADDVAL(res,x,base)
	. set num=$piece((num/2),"."),x=$$ADDVAL(x,x,base)
	quit res
CONVERTBASE(val,frombase,tobase)
	new m,res,power
	set m=$$VALIDLEN(val,frombase)
	set res="0",power=1
	for  quit:m<1  do
	. set res=$$ADDVAL(res,$$MULTIPLYBYNUMBER($extract(val,m),power,tobase),tobase)
	. set power=$$MULTIPLYBYNUMBER(frombase,power,tobase),m=$increment(m,-1)
	quit res
;Takes a base 16 or base 8 number and converts that number
;into its two's compliment representation (flip bits and add one)
CONVNEG(v,base)
	new i,c,ns,charmap,pocharmap
	set charmap="0123456789ABCDEF"
	set:base=8 charmap="01234567"
	set pocharmap=($extract(charmap,2,$length(charmap)))_"0"
	set v=$translate(v,charmap,$reverse(charmap))
	set ns="",c="0"
	for i=0:1:$length(v)  quit:(c'="0")  set c=$extract(v,$length(v)-i),c=$translate(c,charmap,pocharmap),ns=c_ns
	set:i'=$length(v) ns=$extract(v,1,$length(v)-i)_ns
	set:$find(($extract(charmap,1,(base/2))),$extract(ns,1)) ns=($extract(charmap,$length(charmap)))_ns
	quit ns
;Returns length of string until which all characters are valid for the given base (works up to base 16)
VALIDLEN(val,base)
	new i,valbasechar,invalidi,len
	set valbasechar=$extract("0123456789ABCDEF",1,base),len=$length(val)
	quit:'len 0
	for i=1:1:$length(val)  set invalidi='($find(valbasechar,($extract(val,i))))  quit:invalidi
	quit $select($get(invalidi):i-1,1:i)
