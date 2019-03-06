;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;                                                               ;
; Copyright (c) 1987-2018 Fidelity National Information		;
; Services, Inc. and/or its subsidiaries. All rights reserved.	;
;                                                               ;
;       This source code contains the intellectual property     ;
;       of its copyright holder(s), and is made available       ;
;       under a license.  If you do not know the terms of       ;
;       the license, please stop and do not read further.       ;
;                                                               ;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%DO     ;GT.M %DO utility - decimal to octal conversion program
        ;invoke with %DO in decimal and %DL digits to return %DO in octal
        ;invoke at INT to execute interactively
        ;invoke at FUNC as an extrinsic function
        ;if you make heavy use of this routine, consider $ZCALL
        ;
        s %DO=$$FUNC(%DO,$g(%DL))
        q
INT     n %DL
        r !,"Decimal: ",%DO r !,"Digits:  ",%DL s %DO=$$FUNC(%DO,%DL)
        q
FUNCFM(d,l)
	q:d=0 $e("000000000000",1,l)
	n o
	s o=""
	f  q:'d  s o=d#8_o,d=d\8
	q $e("000000000000",1,l-$l(o))_o
FUNC(d,l)
	n isn,i,h,apnd
	s:'$l($g(l)) l=12
	s isn=0,i=0,h="",apnd="0"
	if d["-" do
	. s isn=1,d=$extract(d,2,$l(d))
	if ($l(d)<18) do
	. s h=$$FUNCFM(d,l)
	else  do
	. s h=$$CONVERTBASE^%CONVBASEUTIL(d,10,8)
	if (isn&(h'="0")) do
	. s h=$$CONVNEG^%CONVBASEUTIL(h,8)
	. s apnd="7"
	s i=$l(h)
	f  q:i'<l  s h=apnd_h,i=i+1
	q h
