;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 2001, 2008 Fidelity Information Services, Inc	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
tttgen	;scan the ttt input file and produce c source code
	;zcmdline processing for use in make files
	Set outfile="ttt.c"
	Set zcmdline=$ZCMDLINE
	If $Length(zcmdline) Do
	. Set infile=$Piece(zcmdline," ",1)
	. For i=2,3 Do
	. . Set fullfile=$Piece(zcmdline," ",i)
	. . Set ndirs=$Length(fullfile,"/")
	. . Set filename=$Piece(fullfile,"/",ndirs)
	. . Set loadh(filename)=fullfile
	Else  Do
	. Break
	. Set infile="ttt.txt"
	. Set loadh("opcode_def.h")="opcode_def.h",loadh("vxi.h")="vxi.h"
	Do ^loadop
	Do ^loadvx
	Open infile:read
	Do ^tttscan
	Do ^chkop
	Do ^chk2lev
	Do ^gendash
	Do ^genout
	Quit
