;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 2001, 2007 Fidelity Information Services, Inc	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
genout	;output the results
	o outfile:newv u outfile
	n knt
	s knt=0
	w "/****************************************************************",!
	w " *",$c(9),$c(9),$c(9),$c(9),$c(9),$c(9),$c(9),$c(9),"*",!
	w " *",$c(9),"Copyright 2001, ",$Zdate($H,"YEAR")," Fidelity Information Services, Inc",$c(9),"*",!
	w " *",$c(9),$c(9),$c(9),$c(9),$c(9),$c(9),$c(9),$c(9),"*",!
	w " *",$c(9),"This source code contains the intellectual property",$c(9),"*",!
	w " *",$c(9),"of its copyright holder(s), and is made available",$c(9),"*",!
	w " *",$c(9),"under a license.  If you do not know the terms of",$c(9),"*",!
	w " *",$c(9),"the license, please stop and do not read further.",$c(9),"*",!
	w " *",$c(9),$c(9),$c(9),$c(9),$c(9),$c(9),$c(9),$c(9),"*",!
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
