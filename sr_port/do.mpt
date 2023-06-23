;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;                                                               ;
; Copyright (c) 1987-2019 Fidelity National Information		;
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
        set %DO=$$FUNC(%DO,$get(%DL,12))
        quit
INT     new %DL
        read !,"Decimal: ",%DO read !,"Digits:  ",%DL  set:""=%DL %DL=12 set %DO=$$FUNC(%DO,%DL)
        quit
FUNC(d,l)
	new i,o,s
	set o="",l=$get(l,12),s=0
	if "0"[$get(d) set $piece(s,0,l+1)="" quit s
	set:"-"=$extract(d) s="7",d=$extract(d,2,9999)
	if (18>$length(d)) for  quit:'d  set o=d#8_o,d=d\8
	else  set o=$$CONVERTBASE^%CONVBASEUTIL(d,10,8)
	set:(("7"=s)&("0"'=o)) o=$$CONVNEG^%CONVBASEUTIL(o,8)
	set i=$length(o)
	quit:l'>i o
	set $piece(s,s,l-i+1)=""
	quit s_o
