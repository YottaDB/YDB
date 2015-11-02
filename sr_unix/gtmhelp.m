;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 2002, 2010 Fidelity Information Services, Inc	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
gtmhelp(subtopic,gbldir)
	;
	;
	new (subtopic,gbldir)
	new $ztrap
	set $ztrap="zgoto "_$zl_":error"
	set %gbldir=$zgbldir
	set $zgbldir=gbldir
	set dio=$io
	set COUNT=0,NOTFOUND=0
	do parse(subtopic)
	for  do display quit:COUNT<0
	if ($zsearch(%gbldir)'="") set $zgbldir=%gbldir
	else  set $zgbldir=""
	use dio
	quit
	;
	;
parse(subtopic)
	;
	;
	new (subtopic,NEW,COUNT,TOPIC)
	set NEW=0
	f i=1:1:$length(subtopic," ") set x=$p(subtopic," ",i) if x'="" do
 .		set COUNT=COUNT+1,NEW=NEW+1
 .		set TOPIC(COUNT)=$$UCASE(x)
 .		quit
	quit
	;
	;
display
	;
	;
	new (COUNT,TOPIC,MATCH,PROMPT,NEW,NOTFOUND)
	if $g(TOPIC(COUNT))="?" set COUNT=COUNT-1
	write #
	if $$MATCH do
 .		if NOTFOUND do
 ..			write !!,"Sorry, no Documentation on "
 ..			for i=COUNT+1:1:NEW+COUNT write TOPIC(i)," "
 ..			set NOTFOUND=0
 ..			quit
 .		for i=1:1:MATCH do print(MATCH(i),i)
 .		if $data(@MATCH(MATCH)@("s"))>1&(MATCH=1) do
 ..			write $$FORMAT(4)
 ..			write !!,"Additional information available: ",!!
 ..			set x=""
 ..			set subref=$name(@MATCH(MATCH)@("s"))
 ..			for   set x=$order(@subref@(x)) quit:x=""  do
 ...				write $$FORMAT(0)
 ...				write @subref@(x)
 ...				write $$COLUMNS(subref,x)
 ...				do qualifiers($name(@subref@(x)))
 ...				quit
 ..			quit
 .		else   for i=1:1:NEW  set COUNT=COUNT-1
 .		if $zeof write # set COUNT=COUNT-1 quit
 .		write $$PROMPT
 .		read subtopic,!
 .		if subtopic="" set COUNT=COUNT-1
 .		if subtopic'="" do parse(subtopic)
 .		quit
	else  do
 .		set NOTFOUND=1
 .		for i=1:1:COUNT write TOPIC(i)," "
 .		for i=1:1:NEW set COUNT=COUNT-1
 .		quit
	quit
	;
	;
print(ref,i);
	;
	new (ref,i,MATCH,COUNT)
	write !,@ref
 	set y=""
 	for  set y=$order(@ref@(y)) q:(y="s")!(y="")  do
 .		write $$FORMAT(1)
 .		w !,@ref@(y)
 	if $data(@ref)>1 do
 .		set subref=$name(@ref@("s")),x=""
 .		for  set x=$order(@subref@(x)) q:x=""  do:($e(^(x))="-")
 ..			set MATCH(i)=$name(MATCH(i),COUNT-1*2)
 ..			write $$FORMAT(1)
 ..			write !,@subref@(x)
 ..			set z=""
 ..			for  set z=$order(@subref@(x,z)) q:z=""  do
 ...				write !,@subref@(x,z)
 ...				quit
 ..			quit
 .		quit
 	quit
	;
recursiv(ref,level)
	;
	new (COUNT,TOPIC,ref,MATCH,level,PROMPT,FLAG)
	set level=level+1
 	if ($extract(TOPIC(level))="-")&($get(FLAG)'=1) do
 .		set FLAG=1
 .		for i=COUNT:-1:level set TOPIC(i+1)=TOPIC(i)
 .		set COUNT=COUNT+1
 .		set TOPIC(level)="*"
 .		quit
	set ref=$name(@ref@("s",TOPIC(level)))
	if TOPIC(level)'="" do:$data(@ref)
 .		if level=COUNT do
 ..			set MATCH=MATCH+1
 ..			set MATCH(MATCH)=ref
 ..			quit
 .		if level'=COUNT do recursiv(ref,level)
 .		quit
	if TOPIC(level)="*" set TOPIC(level)=""
	set x=""
	for  set x=$o(@ref) quit:(x="")!("\"_x'[("\"_TOPIC(level)))  do
 .		set ref=$name(@ref,(level*2)-1)
 .		set ref=$name(@ref@(x))
 .		if level=COUNT do
 ..			for j=1:1:COUNT set PROMPT(j)=@$name(@ref,j*2)
 ..		 	set MATCH=MATCH+1
 ..			set MATCH(MATCH)=ref
 ..			quit
 .		if level'=COUNT do recursiv(ref,level)
 .		quit
	quit
qualifiers(ref) ;
	   ;
	new (ref)
	if $data(@ref)>1 do
 .		set ref=$name(@ref@("s")),x="-"
 .		for  s x=$o(@ref@(x)) quit:x=""!($e(x)'="-")   do:($e(^(x))="-")
 ..			set count=$get(count)+1
 ..			if count=1 write !
 ..			write ^(x)
 ..			write $$COLUMNS(ref,x)
 ..			quit
 .		quit
	if $get(count)>0 write !!
	quit
error	; Error handler called by $ztrap
	;
	set $ztrap=""
	write !,"Error in GT.M  help utility"
	set outfile="gtmhelp.dmp"
	open outfile:newversion
	use outfile
	zshow "*"
	close outfile
	use dio
	set $ecode=""
	quit
MATCH() ; Return array MATCH which contains all Global references which match
	; the TOPIC array.
	new (TOPIC,COUNT,MATCH,PROMPT)
	set QUALIFIERS=0
	if COUNT=0 set MATCH=1 set MATCH(1)="^HELP"
	if COUNT'=0 do
 .		set level=0
 .		set MATCH=0
 .		set ref="^HELP"
 .		do recursiv(ref,level)
 .		quit
	if $g(FLAG)=1 set COUNT=COUNT-1
	quit MATCH
WIDTH()	quit 80	; Width of the current device
PAGE()	quit 24	; Page length of the current device
FORMAT(newlines)
	if $y>($$PAGE-newlines-3) do
 .		if '$zeof read !!,"Press RETURN to continue ...",dummy
 .		write #
 .		quit
	quit ""
COLUMNS(subref,x)
	if $x+12'>$$WIDTH write ?$x\12+1*12
	if $x+$l($o(@subref@(x)))>$$WIDTH w !
	if $x+12>$$WIDTH write !
	quit ""
PROMPT()
	new (COUNT,TOPIC,PROMPT)
	write !!
	set ref="^HELP"
 	for i=1:1:COUNT do
 .		set TOPIC(i)=$$UCASE($s($d(PROMPT(i)):PROMPT(i),1:TOPIC(i)))
 .		set ref=$name(@ref@("s",TOPIC(i)))
 .		write @ref," "
 .		quit
 	if COUNT=0 kill PROMPT write "Topic? "
 	if COUNT>0 write "Subtopic? "
	quit ""
UCASE(string)
	set lo="abcdefghijklmnopqrstuvwxyz"
	set up="ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	quit $translate(string,lo,up)

