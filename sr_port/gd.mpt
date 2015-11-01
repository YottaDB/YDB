;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 1987,2001 Sanchez Computer Associates, Inc.	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%GD	;GT.M %GD utility - global directory
	;
	n %ZG,%ZL
        i '$d(%zdebug) n $et s $et="zg "_$zl_":ERR^%GD" u $p:(ctrap=$c(3):exc="k %ZG zg "_$zl_":LOOP^%GD")
	w !,"Global Directory",!
	d GD^%GSEL,EXIT
	q
ERR	u $p w !,$p($zs,",",2,99),!
	s $ec=""
	; Warning: Fall-through!
EXIT	u $p:(ctrap="":exc="")
	q
LOOP	d GD^%GSEL,EXIT
	q
