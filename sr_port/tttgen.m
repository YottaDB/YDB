;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 2001 Sanchez Computer Associates, Inc.	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
tttgen	;scan the ttt input file and produce c source code
	Break
	Set infile="ttt.txt",outfile="ttt.c"
	Do ^LOADOP
	Do ^LOADVX
	Open infile:read
	Do ^TTTSCAN
	Do ^CHKOP
	Do ^CHK2LEV
	Do ^GENDASH
	Do ^GENOUT
	Quit
