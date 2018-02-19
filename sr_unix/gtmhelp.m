;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
; Copyright (c) 2002-2018 Fidelity National Information		;
; Services, Inc. and/or its subsidiaries. All rights reserved.	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
gtmhelp(subtopic,gbldir)
	;
	new (gbldir,subtopic)
	new $zgbldir						; New $zgbldir so it is restored on exit
	zshow "d":zshow						; to capture original $P state - assumes $P is always subscript 1
	set IO(1)=$io						; to capture the original $IO
	use $principal:nocenable				; do as soon as there's hope of undoing it, ie, after 2 prior lines
	set pio=$zwrite($io)					; in case of an early error
	new $etrap						; also inportant to get in place
	set $etrap="zgoto "_$zlevel_":error^GTMHELP"		; the code below sets up pio to restore the original $P state
	set:$io'=IO(1) IO(0)=$io				; now capture $P state - IO(0) means there' a device other than $P
	; Capture the existing $principal's state to restore on exit
	set tmp="$principal:("_$select(zshow("D",1)["NOCENE":"no",1:"")_"cenable"
	set tmp=tmp_":ctrap="_$select(zshow("D",1)["CTRA=":$piece($piece(zshow("D",1),"CTRA=",2)," "),1:"""""")
	set exception=$zwrite($select(zshow("D",1)["EXCE=""":$piece(zshow("D",1),"EXCE=",2),1:""""""))	; assumes EXCE last
	set tmp=tmp_":exception="_$extract(exception,3,$length(exception)-2)
	set pio=tmp_")"
	; Override the exception handler
	if zshow("D",1)["TERMINAL" do
	. use $principal:(ctrap=$char(3):exception="zgoto "_$zlevel_":again:$zstatus[""CTRAP"","_$zlevel_":error^GTMHELP")
	else  use $principal:(exception="zgoto "_$zlevel_":error^GTMHELP")
	set $zgbldir=gbldir
again	set:$data(COUNT) subtopic=""				; <CTRL-C> comes back to here and clears any topic
	kill (%gbldir,IO,pio,subtopic)				; X-NEW is evil, but performance is not an issue here
	set COUNT=0,IO="",NOTFOUND=0				; some initialization
	do parse(subtopic)					; check for topic passed in
	for  do display quit:COUNT<0				; drive the real work
	use @pio,IO(1)						; restore $P state and original $IO
	quit
	;
parse(subtopic)							; organize space-delimited input memes into a topic hierarchy
	new (pio,subtopic,NEW,COUNT,TOPIC)
	set NEW=0
	for i=1:1:$length(subtopic," ") set x=$piece(subtopic," ",i) if x'="" do
	. set COUNT=COUNT+1,NEW=NEW+1
	. set TOPIC(COUNT)=$zconvert(x,"U")
	. quit
	quit
	;
display								; do the real work
	new (pio,COUNT,IO,MATCH,NEW,NOTFOUND,PROMPT,TOPIC)
	if $get(TOPIC(COUNT))="?" set COUNT=COUNT-1		; refresh choices on the same topic (leve)
	write #
	if $$MATCH do						; look up the topic
	. if NOTFOUND do
	. . write !!,"Sorry, no Documentation on "
	. . for i=COUNT+1:1:NEW+COUNT write TOPIC(i)," "
	. . set NOTFOUND=0
	. . quit
	. for  set IO=$order(IO(IO),-1) quit:""=IO  do		; if juggling devices, send content to both; do $P 2nd
	. . use IO(IO)
	. . for i=1:1:MATCH do print(MATCH(i),i)		; drive out lines of text using print
	. . quit
	. if $data(@MATCH(MATCH)@("s"))>1&(MATCH=1) do		; if descendent topics show the choices only on $P
	. . write $$FORMAT(4)
	. . write !!,"Additional information available: ",!!
	. . set x=""
	. . set subref=$name(@MATCH(MATCH)@("s"))
	. . for   set x=$order(@subref@(x)) quit:x=""  do		; use a "tabbed" list display
	. . . write $$FORMAT(0)
	. . . write @subref@(x)
	. . . write $$COLUMNS(subref,x)
	. . . do qualifiers($name(@subref@(x)))
	. . . quit
	. . quit
	. else   set COUNT=COUNT-NEW			; otherwise, reposition to the start of the current level
	. if $zeof write # set COUNT=COUNT-1 quit 	; No more input, peel back out write # could cause an error
	. write $$PROMPT
	. read subtopic,!
	. if subtopic="" set COUNT=COUNT-1 quit:0>COUNT	; no answer means peal back a level
	. else  do parse(subtopic)			; check out the response
	. quit
	else  do						; look up failed
	. set NOTFOUND=1					; flag the next call
	. set COUNT=COUNT-NEW				; reset to the last working level
	. quit
	quit
	;
print(ref,i);							; text output function
	new (pio,ref,i,MATCH,COUNT)
	write !,@ref
	set y=""
	for  set y=$order(@ref@(y)) quit:(y="s")!(y="")  do
	. write $$FORMAT(1)
	. write !,@ref@(y)
	. quit
	if $data(@ref)>1 do
	. set subref=$name(@ref@("s")),x=""
	. for  set x=$order(@subref@(x)) quit:x=""  do:($extract(^(x))="-") 	; do lines at this level
	. . set MATCH(i)=$name(MATCH(i),COUNT-1*2)
	. . write $$FORMAT(1)
	. . write !,@subref@(x)
	. . set z=""
	. . for  set z=$order(@subref@(x,z)) quit:z=""  do			; and its descendents
	. . . write !,@subref@(x,z)
	. . . quit
	. . quit
	. quit
	quit
	;
recursiv(ref,level)						;
	new (pio,COUNT,TOPIC,ref,MATCH,level,PROMPT,FLAG)
	set level=level+1
	if ($extract(TOPIC(level))="-")&($get(FLAG)'=1) do
	. set FLAG=1
	. for i=COUNT:-1:level set TOPIC(i+1)=TOPIC(i)
	. set COUNT=COUNT+1
	. set TOPIC(level)="*"
	. quit
	set ref=$name(@ref@("s",TOPIC(level)))
	if TOPIC(level)'="" do:$data(@ref)
	. if level=COUNT do
	. . set PROMPT(level)=TOPIC(level)
	. . set MATCH=MATCH+1
	. . set MATCH(MATCH)=ref
	. . quit
	. if level'=COUNT do recursiv(ref,level)
	. quit
	if TOPIC(level)="*" set TOPIC(level)=""
	set x=""
	for  set x=$order(@ref) quit:(x="")!("\"_x'[("\"_TOPIC(level)))  do
	. set ref=$name(@ref,(level*2)-1)
	. set ref=$name(@ref@(x))
	. set (TOPIC(level),PROMPT(level))=@$name(@ref,level*2)
	. if level=COUNT do
	. . set MATCH=MATCH+1
	. . set MATCH(MATCH)=ref
	. . quit
	. if level'=COUNT do recursiv(ref,level)
	. quit
	quit
qualifiers(ref)							; qualifier lister
	new (pio,ref)
	if $data(@ref)>1 do
	. set ref=$name(@ref@("s")),x="-"
	. for  set x=$order(@ref@(x)) quit:x=""!($extract(x)'="-")   do:($extract(^(x))="-")
	. . set count=$get(count)+1
	. . if count=1 write !
	. . write ^(x)
	. . write $$COLUMNS(ref,x)
	. . quit
	. quit
	if $get(count)>0 write !!
	quit
	;
error								; Error handler called by $etrap
	if ($zstatus'["IOEOF") do				; EOF is not a "real" error
	. write !,"Error in GT.M help utility - look at ",$zjobexam("gtmhelpdmp")," for additional information",!
	. quit
	use @pio,IO(1)						; restore $P state and original $IO
	set $ecode=""						; caller loses error trace, but generally called by direct mode
	quit
MATCH()								; return array MATCH containing all global references that match
	; the TOPIC array.
	new (pio,TOPIC,COUNT,MATCH,PROMPT)
	set QUALIFIERS=0
	if COUNT=0 set MATCH=1 set MATCH(1)="^HELP"
	if COUNT'=0 do
	. set level=0
	. set MATCH=0
	. set ref="^HELP"
	. do recursiv(ref,level)
	. quit
	if $get(FLAG)=1 set COUNT=COUNT-1
	quit MATCH
	;
WIDTH()	quit 80	; Width of the current device
PAGE()	quit 24	; Page length of the current device
FORMAT(newlines)
	if $y>($$PAGE-newlines-3) do
	. if '$zeof read !!,"Press RETURN to continue ...",dummy
	. write #
	. quit
	quit ""
COLUMNS(subref,x)
	if $x+12'>$$WIDTH write ?$x\12+1*12
	if $x+$l($order(@subref@(x)))>$$WIDTH write !
	if $x+12>$$WIDTH write !
	quit ""
PROMPT()
	new (pio,COUNT,TOPIC,PROMPT)
	write !!
	set ref="^HELP"
	for i=1:1:COUNT do
	. set TOPIC(i)=$zconvert($select($data(PROMPT(i)):PROMPT(i),1:TOPIC(i)),"U")
	. set ref=$name(@ref@("s",TOPIC(i)))
	. write @ref," "
	. quit
	if COUNT=0 kill PROMPT write "Topic? "
	if COUNT>0 write "Subtopic? "
	quit ""
