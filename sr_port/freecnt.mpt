;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 1989,2001 Sanchez Computer Associates, Inc.	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%FREECNT;GT.M %FREECNT utility - display database free blocks
	;
	n rn,fn,fb,tb,%ZL
        i '$d(%zdebug) n $et s $et="zg "_$zl_":ERR^%FREECNT" u $p:(ctrap=$c(3):exc="zg "_$zl_":EXIT^%FREECNT")
	s rn=$view("GVFIRST")
	d head,show
	f  s rn=$view("gvnext",rn) q:rn=""  d show
	d EXIT
	q
head	;
	w "Region",?16,"Free",?25,"Total",?40,"Database file",!,"------",?16,"----",?25,"-----",?40,"-------------",!
	q
show	;
	s fn=$v("GVFILE",rn),fb=$v("FREEBLOCKS",rn),tb=$v("TOTALBLOCKS",rn)
	w rn,?12,$j(fb,8),?22,$j(tb,8)," (",$j(fb/tb*100.0,5,1),"%)",?40,fn,!
	q
ERR	w !,$p($zs,",",2,99),!
	s $ec=""
	; Warning: Fall-through
EXIT	u $p:(ctrap="":exc="")
	q
