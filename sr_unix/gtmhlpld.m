;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 2002 Sanchez Computer Associates, Inc.	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
gtmload	;
	new (%zdebug)
	set %level=$zlevel
	set $ztrap=$select($data(%zdebug):"b",1:"zg %level:err")
	f  read "Help file > ",file  s x=$zsearch(file) q:file=""!(x'="")   d
 .		w !,"File does not exist, <CR> to quit.",!
 .		quit
	if file="" quit
	open file:(readonly)
	use file
	set ref="^HELP",incr=1,count=0,level=0,oldlevel=0,^HELP=""
	for  read line quit:$zeof  do
 .		set first=$extract(line)
 .		if first?1N!(first="-") do
 ..			if first?1N set level=first,oldlevel=first
 ..			else  set level=oldlevel+1
 ..			if level'>count set ref=$name(@ref,(level-1)*2)
 ..			set count=level
 ..			set:first?1N subtopic=$piece(line," ",2)
 ..			set:first="-" subtopic=$piece(line," ",1)
 ..			set ref=$name(@ref@("s"))
 ..			set ref=$name(@ref@($$UCASE(subtopic)))
 ..			set @ref=subtopic
 ..			set incr=1
 ..			quit
 .		else  do
 ..			set @ref@(incr)=line
 ..			set incr=incr+1
 ..			quit
 .		quit
	close file
 	quit
err	;
	set $ztrap=""
	u 0 w !,"Error in GT.M help load utility."
	set file="gtmhlpld.dmp"
	open file:newversion
	use file
	zshow "*"
	close file
	quit
	;
UCASE(string)
	set lo="abcdefghijklmnopqrstuvwxyz"
	set up="ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	quit $translate(string,lo,up)

