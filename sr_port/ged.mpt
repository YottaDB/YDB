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
%ged	;%ged - global editor
	n (%zdebug)
	s %("lo")="abcdefghijklmnopqrstuvwxyz"
	s %("up")="ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	s %("$zso")=$zso
	s %("$io")=$io
	s $zso=$select($zv'["VMS":".",1:"[]")_"_"_$j_".ged"
	s %("zlbeg")=$zl
beg	;
	n $et  s $et=$s($d(%zdebug):"b",1:"s $ec="""" zg "_$zl_":beg")
	u 0:(ctrap=$c(3))
	f  r !,"Edit ^",%("global"),! q:%("global")=""  d
 .		i $e(%("global"))'="^" s %("global")="^"_%("global")
 .		if (($e(%("global"),2)="%")&($l(%("global"))=2)) d   q
 ..			w !,"%ged will not edit: ",%("global")
 ..			q
 .		if ($e(%("global"),2)="?") d help q
 .		d convert
 .		d main
 .		k @%("local"),@(%("a")_$e(%("global"),3,%("glbl lngth")))
 .		zwi %("add"),%("change"),%("kill"),%("s")
 .		quit
	s $zso=%("$zso") u %("$io"):(ctrap="":exception="")
	q
	;
main	;
	s %("glbl lngth")=$f(%("global"),"(")-2
	if %("glbl lngth")=-2 s %("glbl lngth")=$l(%("global"))
	s %("local")=$e(%("global"),2,%("glbl lngth"))
	d zwr
	s %=%("global")
	o $zso u $zso
	f  r % q:$zeof  s @(%("a")_$e(%,3,9999)) s %("s")=$g(%("s"))+1
	c $zso
	zed $zso
	o $zso:(read:rewind)
	d stor
	c $zso:delete
	o $zso c $zso:delete
	w !,"node          : ",%("global")
	w !,"selected      : ",+$g(%("s"))
	w !,"changed       : ",+$g(%("change"))
	w !,"added         : ",+$g(%("add"))
	w !,"killed        : ",+$g(%("kill"))
	q
	;
zwr	;
	n $et s $et=$s($d(%zdebug):"b",1:"s $ec="""" goto continue")
	o $zso:(newversion:exc="":noreadonly)
	u $zso zwr @%("global")
continue
	c $zso
	q
badzwr	;
	c $zso:delete
	u 0 w !,"invalid global description: ",%("global")
	q
stor	;
	s %("storlvl")=$zl
	u $zso
	f  r % quit:$zeof  d istor
	do work
	q
istor	;
	n $et
	s $et="do istorerr"
	i '$l($tr(%," ")) q
	i $e(%)'="^" s %="^"_%
	i $e(%,1,%("glbl lngth"))=$e(%("global"),1,%("glbl lngth")) do
 .		s %=$e(%,2,99999)
 .		s @%
 .		quit
	else   	do
 .		set %("cur io")=$io
 .		u 0 w !,"%ged will not edit: ",%
 .		use %("cur io")
 .		quit
	q
	;
istorerr;
	close $zso
	use 0
	w !,"invalid syntax: ",%,!,"return to continue: "  r %:30,!
	zed $zso
	open $zso:(read:rewind)
	s $ec=""
	zg %("storlvl"):stor
work	;
	s %=%("local")
	if $d(@%)'[0          d check 		; Set top node
	f  s %=$q(@%) q:%=""  d check
	d kill
 	q
check	;
	if ($d(@("^"_%))[0)&($d(@(%("a")_$e(%,2,9999)))[0) d add  q
	if ($d(@("^"_%))[0)&($d(@(%("a")_$e(%,2,9999)))'[0) d addcheck  q
	if ($d(@("^"_%))'[0)&($d(@(%("a")_$e(%,2,9999)))[0) d conflict q
	if @("^"_%)=@% d withdraw q
	if @(%("a")_$e(%,2,9999))=@("^"_%) d change q
	d checkerr
	q
	;
add	;
	s @("^"_%)=@%
	s %("add")=$g(%("add"))+1
	d withdraw
	q
	;
change	;
	s @("^"_%)=@%
	s %("change")=$g(%("change"))+1
	d withdraw
	q
	;
withdraw;
	if $d(@(%("a")_$e(%,2,9999)))'[0 zwithdraw @(%("a")_$e(%,2,9999))
	q
	;
conflict;
	u 0
	w !,"WARNING: The original value for node ^",%," was not stored before"
	w !,"the edit session and may have changed while in the editor.",!
	s %("%local")=%("a")_$e(%,2,9999)
	w !,"old value     : ",$s($d(@%("%local"))'[0:@%("%local"),1:"")
	w !,"current value : ",@("^"_%)
	w !,"edit value    : ",@(%),!!
	w !,"Do you still wish to use the edit value? [n] "
	r %("ans"):30 s %("ans")=$tr(%("ans"),%("lo"),%("up"))
	if "\NO"[("\"_%("ans")) d withdraw q
	if "\YES"[("\"_%("ans")) d change q
	d withdraw
	q
addcheck;
	u 0
	w !,"WARNING: Node ^",%," was deleted while in the editor",!
	s %("%local")=%("a")_$e(%,2,9999)
	w !,"old value     : ",$s($d(@%("%local"))'[0:@%("%local"),1:"")
	w !,"current value : "
	w !,"edit value    : ",@(%),!!
	w !,"Do you still wish to use the edit value? [n] "
	r %("ans"):30 s %("ans")=$tr(%("ans"),%("lo"),%("up"))
	if "\NO"[("\"_%("ans")) d withdraw q
	if "\YES"[("\"_%("ans")) d add q
	d withdraw
	q
checkerr;
	u 0
	w !,"WARNING: Node ^",%," was modified while in editor",!
	s %("%local")=%("a")_$e(%,2,9999)
	w !,"old value     : ",$s($d(@%("%local"))'[0:@%("%local"),1:"")
	w !,"current value : ",@("^"_%)
	w !,"edit value    : ",@(%),!!
	w !,"Do you still wish to use the edit value? [n] "
	r %("ans"):30 s %("ans")=$tr(%("ans"),%("lo"),%("up"))
	if "\NO"[("\"_%("ans")) d withdraw q
	if "\YES"[("\"_%("ans")) d add q
	d withdraw
	q
	;
kill	;
	n $et
	s $et="s $ec="""" q"
	s %=(%("a")_$e(%("local"),2,9999))
	if $d(@%)'[0 d killcheck
	f  s %=$q(@%) q:%=""  d killcheck
	q
	;
killcheck;
	if $d(@("^"_%("b")_$e(%,2,9999)))[0 s %("kill")=$g(%("kill"))+1 q
	if @("^"_%("b")_$e(%,2,9999))=@% zwi @("^"_%("b")_$e(%,2,9999)) s %("kill")=$g(%("kill"))+1 q
	u 0
	w !,"killed node has changed: "
	w !,"node          :  ^",%("b")_$e(%,2,9999)
	w !,"old value     :   ",@(%("a")_$e(%,2,9999))
	w !,"current value :   ",@("^"_%("b")_$e(%,2,9999))
	q
	;
nofile	c $zso:delete
	u 0 w !,"No changes made",!
	q
	;
convert;
	s %("b")=$e(%("global"),2)
	s %("a")=$a(%("global"),2)
	s %("a")=%("a")+1
	s %("a")=$s(%("a")=91:65,%("a")=123:97,%("a")=38:65,1:%("a"))
	s %("a")=$c(%("a"))
	q
	;
help	i "dD"[$e(%("global"),3),$l(%("global"))=3 d ^%GD q
	w !,"VALID INPUT",!!
	w !,?3,"<RET>",?16,"to leave the %GED utility ",!
	w !,?4,"?D",?16,"to display existing globals in your directory ",!
	w !,"[global name]",?16,"the MUMPS name for the global e.g. ABC"
	w !?16,"the global name may be followed by: "
	w !?16,"subscript(s) in parentheses"
	w !?16,"a subscript is a MUMPS expression e.g. ""joe"",10,$e(a,1),"
	w !?16,"a ""*"" as a subscript causes all descendents to be included,"
	w !?16,"or by a range of subscripts in parentheses"
	w !?16,"expressed as [expr]:[expr] e.g 1:10 ""a"":""d"""
	w !?16,"a MUMPS pattern to match selected subscripts: ^TEST(?1.3N)"
	q
