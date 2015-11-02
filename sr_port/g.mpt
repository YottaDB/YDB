;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 1987, 2009 Fidelity Information Services, Inc	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%G	;GT.M %G utility - global lister
	;
	n %in,%ZL,%ZD
        i '$d(%zdebug) n $et s $et="zg "_$zl_":ERR^%G" u $p:(ctrap=$c(3):exc="zg "_$zl_":LOOP^%G")
	f  d  q:$l(%ZD)
        . r !,"Output device: <terminal>: ",%ZD,!
        . i '$l(%ZD) s %ZD=$p q
        . i %ZD="^" q
        . i %ZD="?" d  q
         . . w !!,"Select the device you want for output"
         . . w !,"If you wish to exit enter a carat (^)",!
	 . . s %ZD=""
        . i $zparse(%ZD)="" w "  no such device" s %ZD="" q
        . o %ZD:(newversion:block=2048:record=2044:exception="g noopen"):0
        . i '$t  w !,%ZD," is not available" s %ZD="" q
        . q
noopen  . w !,$p($ZS,",",2,999),! c %ZD s %ZD=""
        q:%ZD="^"
	d base
	q
base	f  r !,"List ^",%in,! q:%in=""  d
	. i $e(%in)="?",$l(%in)=1 d help q
	. i (%in="?D")!(%in="?d") d ^%GD u $p:(ctrap=$c(3):exc="zg "_($zl-2)_":LOOP^%G") q
	. s:%in="*" %in="?.E(*)"
	. s:$p(%in,"(")="*" $p(%in,"(")="?.E"
	. s:$e(%in)'="^" %in="^"_%in
	. n $et s $et="ZG "_$ZL_":badzwr"
	. u %ZD zwr @%in u $p
	. q
badzwr	. u $p w !,$p($zs,",",3,99),!
	. s $ec=""
	d EXIT
	q
help	w !,"VALID INPUT",!!
	w !,?3,"<RET>",?16,"to leave the %G utility ",!
	w !,?4,"?D",?16,"to display existing globals in your directory ",!
	w !,"[global name]",?16,"the MUMPS name for the global e.g. ABC, or"
	w !?16,"a MUMPS pattern to match selected globals e.g. ?1""A"".E, or"
	w !?16,"""*"" as a wildcard for all globals"
	w !?16,"the global name may be followed by: "
	w !?16,"subscript(s) in parentheses"
	w !?16,"a subscript is a MUMPS expression e.g. ""joe"",10,$e(a,1),"
	w !?16,"a ""*"" as a subscript causes all descendents to be included,"
	w !?16,"or by a range of subscripts in parentheses"
	w !?16,"expressed as [expr]:[expr] e.g 1:10 ""a"":""d""",!
	q
ERR	u $p w !,$p($zs,",",2,99),!
	s $ecode=""
	; Warning - Fall-through
EXIT	i $d(%ZD),%ZD'=$p c %ZD
	u $p:(ctrap="":exc="")
	q
LOOP	if 1'=$zeof d base
	q
