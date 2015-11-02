;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 1989, 2006 Fidelity Information Services, Inc	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%UCASE	;GT.M %UCASE utility - convert a string to all upper case
	;invoke with string in %S to return upper case string in %S
	;invoke at INT to execute interactively
	;invoke at FUNC as an extrinsic function
	;if you make heavy use of this routine, consider $ZCALL
	;
	s %S=$$FUNC(%S)
	q
INT	n %S
	r !,"String: ",%S w !,"Upper: ",$$FUNC(%S),!
	q
FUNC(s)
	new returnM,returnUTF8,ret,index
	i ($zver["VMS")!($ZCHSET="M") do  quit ret
	.	s returnM="set ret=$tr(s,""abcdefghijklmnopqrstuvwxyz"
	.	for index=224:1:239,241:1:253 s returnM=returnM_$char(index)
	.	s returnM=returnM_""","
	.	s returnM=returnM_"""ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	.	for index=192:1:207,209:1:221 s returnM=returnM_$char(index)
	.	s returnM=returnM_""")"
	.	xecute returnM
	s returnUTF8="set ret=$zconvert(s,""U"")"
	xecute returnUTF8
	q ret
