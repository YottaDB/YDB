;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 1988, 2003 Sanchez Computer Associates, Inc.	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%RSE	;GT.M %RSE utility - find every occurrence of a string in one or more routines
	;invoke ^%RSE to get interaction
	;invoke CALL^%RSE with %ZF - string to find, %ZR - routine array or name, %ZD a device to receive a trail
	;
	n cnt1,cnt2,cnt3,flen,fnd,gtmvt,h,i,o,out,outd,p,r,rl,sx,tics,x,xn,%ZC,%ZD,%ZF,%ZR
	w !,"Routine Search for Every occurrence",!
	s %ZC=0,(cnt1,cnt2,cnt3)=0,(out,outd)=1
	i '$d(%zdebug) n $et s $et="zg "_$zl_":ERR^%RCE" u $p:(ctrap=$c(3):exc="zg "_$zl_":LOOP^%RCE")
	d MAIN^%RCE
	u $p:(ctrap="":exc="")
	q
CALL	i '$l($g(%ZF)) q
	n %ZC,cnt1,cnt2,cnt3,flen,fnd,gtmvt,h,i,o,out,outd,p,r,rl,sx,tics,x,xn n:'$d(%ZD) %ZD
	s %ZD=$g(%ZD),(%ZC,cnt1,cnt2,cnt3,out)=0,outd=$l(%ZD)
	i $d(%ZR)<10 d CALL^%RSEL
	d work^%RCE
	q
