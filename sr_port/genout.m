;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
; Copyright (c) 2001, 2015 Fidelity National Information	;
; Services, Inc. and/or its subsidiaries. All rights reserved.	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
genout	;output the results
	o outfile:newv
	n knt
	s knt=0
	s vms=$zversion["VMS"
	i vms s cfile=$ztrnlnm("gtm$src")_"copyright.txt"
	i 'vms s cfile=$ztrnlnm("gtm_tools")_"/copyright.txt"
	s xxxx="2001"
	s yyyy=$zdate($H,"YYYY")
	o cfile:read
	u outfile w "/****************************************************************",!
	f i=1:1 u cfile r line q:$zeof  d
	. i (1<$zl(line,"XXXX")) d
	. . s str=$zpiece(line,"XXXX",1)_xxxx_$zpiece(line,"XXXX",2)
	. . s str=$zpiece(str,"YYYY",1)_yyyy_$zpiece(str,"YYYY",2)
	. e  d
	. . s str=line
	. u outfile w " *"_str_"*",!
	c cfile
	u outfile
	w " ****************************************************************/",!!
	f i="mdef.h","vxi.h","vxt.h","xfer_enum.h" w "#include """,i,"""",!
	w "LITDEF short ttt[",ttt,"] = {",!
	f i=0:1:ttt-2 d prnt
	w ttt(ttt-1),"};",!
	c outfile
	q
prnt	i knt=0 w !,"/*",$j(i,5)," */",$c(9)
	w ttt(i),","
	s knt=knt+1
	i knt>7!(ttt(i)="VXT_END") s knt=0
	q
