;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
; Copyright (c) 1987-2019 Fidelity National Information		;
; Services, Inc. and/or its subsidiaries. All rights reserved.	;
;								;
; Copyright (c) 2017-2020 YottaDB LLC and/or its subsidiaries.	;
; All rights reserved.						;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%HD	; YottaDB %HD utility - hexadecimal to decimal conversion program
	; invoke at INT with %HD in hexadecimal to return %HD in decimal
	; invoke at FUNC as an extrinsic function
	; if you make heavy use of this routine, consider an external call.
	;
	set %HD=$$FUNC(%HD)
	quit
INT	read !,"Hexadecimal: ",%HD set %HD=$$FUNC(%HD)
	quit
FUNC(h)
	new max set max="ffffffffffffffff"
	quit:"-"=$zextract(h,1) ""
	new d set d=$zextract(h,1,2) set:("0x"=d)!("0X"=d) h=$zextract(h,3,$zlength(h))
	new fval set fval=$zextract(h,1) ; first value
	set:fval="0" h=$$VALIDVAL(h) ; removing leading zero's if present
	; Input greater than max hex value is handled by M implementation.
	; As max value has lowercase f which is higher in ascii value, f or F doesnt make a difference in max comparison.
	; Conversion from lower to upper is done in the else part as required by M implementation.
	if (($length(h)<16)!(($length(h)=16)&(h']max))) do
        . set d=$zconvert(h,"HEX","DEC")
	else  do
	. set h=$translate(h,"abcdef","ABCDEF")
	. set d=$$CONVERTBASE^%CONVBASEUTIL(h,16,10)
	quit d
; This routine removes any leading zero's in the input
; Returns: value without the leading zeros if any
VALIDVAL(val)
        new i,inv,res     ; inv is used to exit the following for on occurence of a valid decimal digit
        set i=1,inv=0,res=0
        for i=1:1:$zlength(val)  do  quit:inv=1
        . set res=$zextract(val,i)
        . set:res'=0 inv=1
        . quit:inv=1
        set:res'=0 res=$extract(val,i,$zlength(val))
        quit res
