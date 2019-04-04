;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
; Copyright (c) 1987-2018 Fidelity National Information		;
; Services, Inc. and/or its subsidiaries. All rights reserved.	;
;								;
; Copyright (c) 2017-2019 YottaDB LLC and/or its subsidiaries.	;
; All rights reserved.						;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%HD	;YottaDB %HD utility - hexadecimal to decimal conversion program
	;invoke at INT with %HD in hexadecimal to return %HD in decimal
	;invoke at FUNC as an extrinsic function
	;if you make heavy use of this routine, consider an external call.
	;
	s %HD=$$FUNC(%HD)
	q
INT	r !,"Hexadecimal: ",%HD s %HD=$$FUNC(%HD)
	q
FUNCFM(h)
	n c,d,dg
	s d=0,h=$tr(h,"abcdef","ABCDEF")
	f c=1:1:$l(h) s dg=$f("0123456789ABCDEF",$e(h,c)) q:'dg  s d=(d*16)+(dg-2)
	q d
FUNC(h)
	q:"-"=$ze(h,1) ""
	n d s d=$ze(h,1,2) s:("0x"=d)!("0X"=d) h=$ze(h,3,$zlength(h))
	q:$l(h)<15 $$FUNCFM(h)
	s h=$tr(h,"abcdef","ABCDEF")
	q $$CONVERTBASE^%CONVBASEUTIL(h,16,10)
