;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
; Copyright (c) 1989-2015 Fidelity National Information		;
; Services, Inc. and/or its subsidiaries. All rights reserved.	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%GC	;GT.M %GC utility - global copy
	;
	new %GI,%GO,%SC,d,x ; $etrap set %ZL=$zlevel;,$etrap="zgoto "_$zlevel_":ERR^%GC"
	set d("io")=$io
	use $principal
	write !,"Global copy",!
	set $zstatus=""
	if '$data(%zdebug) new $etrap set $etrap="zgoto "_$zlevel_":err^"_$text(+0) do
	. zshow "d":d									; save original $p settings
	. set x=$piece($piece(d("D",1),"CTRA=",2)," ")
	. set:""=x x=""""""
	. set d("use")="$principal:(ctrap="_x_":exception=",x=$piece(d("D",1),"EXCE=",2),x=$zwrite($extract(x,2,$length(x)-1))
	. set:""=x x=""""""
	. set d("use")=d("use")_x_":"_$select($find(d("D",1),"NOCENE"):"nocenable",1:"cenable")_")"
	. set x="set $ecode="""" zgoto "_$stack_":RESTART^%GC:$zstatus[""CTRAP"","_$stack_":err^"_$text(+0)
	. use $principal:(ctrap=$char(3,4):exception=x:nocenable)
RESTART	read !,"Show copied nodes <Yes>? ",%SC set %SC=($translate(%SC,"yes","YES")=$extract("YES",1,$length(%SC)))
	for  read !,"From global ^",%GI quit:%GI=""  do COPY
	use:$data(d("use")) @d("use")
	use:$data(d("io")) d("io")
	quit
COPY	new c,ix
	if $extract(%GI)="?" set ix=%GI do help quit
	set:$extract(%GI)'="^" %GI="^"_%GI
	do  quit:""=%GI
	. new $etrap
	. set $etrap="write !,$piece($zstatus,"","",2,99),! set $ecode="""",%GI=""""",x=$qlength(%GI)
	if '$data(@%GI) write !,"Global ",%GI," does not exist." quit
	for  read !,"To global   ^",%GO,! quit:$extract(%GO)'="?"  set ix=%GO do help
	quit:%GO=""
	set:$e(%GO)'="^" %GO="^"_%GO
	do  quit:""=%GO
	. new $etrap
	. set $etrap="write !,$piece($zstatus,"","",2,99),! set $ecode="""",(%GI,%GO)=""""",x=$qlength(%GO)
	if $data(@%GO) write !,"Global ",%GO," already exists." quit
	merge @%GO=@%GI
	zwrite:$get(%SC) @%GO			; comment out the next 2 lines if you don't want to spend the time to get a count
	if $data(@%GI)'[0,$increment(c)
	for  set %GI=$query(@%GI) quit:%GI=""  if $increment(c)
	write "Total ",c," nodes copied.",!
	quit
help	if $length(ix)=2,"Dd"[$extract(ix,2) do ^%GD use $principal:flush quit
	write !!,"This routine copies a node and all its descendents"
	write !,"from one global variable to another"
	write !,"""From global"" requests the source for the copy,"
	write !,"""To global"" requests the destination"
	write !,"Use standard MUMPS gvn syntax to specify the node names"
	write !,"?D invokes %GD to get a global directory"
	write !,"<RETURN> drops you back to the prior prompt or out of %GC"
	write !
	quit
err	write !,$piece($zstatus,",",2,99),!
	use:$data(d("use")) @d("use")
	use:$data(d("io")) d("io")
	set $ecode=""
	quit
