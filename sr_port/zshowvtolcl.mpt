;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
; Copyright (c) 2015 Fidelity National Information		;
; Services, Inc. and/or its subsidiaries. All rights reserved.	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%ZSHOWVTOLCL(%ZSHOWvbase)	; retrieve locals from a zshow probably in a global
	; %ZSHOWvbase is the target location of the zshow "v" and the only thing this can't and won't restore; failure to do so
	; produces a message, but is not considered an error that stops processing by this utility.
	; in order to have a minimum restriction on what it can instantiate for local variables, this uses a single array with a
	; weird name, which makes it not much fun to read (sorry).
	; It may be invoked either as an extrinsic that returns TRUE (1) for success or FALSE (0) for failure, or with a DO.
	; As of this writing, it is expected to be typically a programmer's tool so, while it does a reasonable amount of condition
	; validation, it does not currently handle any GT.M errors, most likely due to input that's badly formed and fails when used
	; for indirection; rather it relies on the current error trap, and it sends its messages of validation issues to the
	; principal device; see sendmsg below if you want to change that.
	if ""'=$ecode do sendmsg("m0") quit:$quit 0 quit			; want to start "error free"
	if "^"'=$extract($qsubscript(%ZSHOWvbase,0)) do sendmsg("m1",%ZSHOWvbase) quit:$quit 0 quit
	set %ZSHOWvbase("x")=%ZSHOWvbase
	set %ZSHOWvbase("depth")=$qlength(%ZSHOWvbase("x"))+1
	set %ZSHOWvbase("length")=$length(%ZSHOWvbase("x"))-1
	for %ZSHOWvbase("i")=1:1:%ZSHOWvbase("depth") quit:"V"=$qsubscript(%ZSHOWvbase("x"),%ZSHOWvbase("i"))
	if %ZSHOWvbase("i")=%ZSHOWvbase("depth") do			; add a trailing "V"
	. if %ZSHOWvbase("x")["(" set %ZSHOWvbase("x")=$extract(%ZSHOWvbase("x"),1,%ZSHOWvbase("length"))_",""V"")"
	. else  set %ZSHOWvbase("x")=%ZSHOWvbase("x")_"(""V"")"
	. set %ZSHOWvbase("depth")=%ZSHOWvbase("depth")+1
	else  if %ZSHOWvbase("i")+1=%ZSHOWvbase("depth"),"V"=$qsubscript(%ZSHOWvbase("x"),%ZSHOWvbase("depth")-1)	;just fine
	else  if %ZSHOWvbase("i")+2=%ZSHOWvbase("depth"),1=$qsubscript(%ZSHOWvbase("x"),%ZSHOWvbase("depth")-1) do	; strip 1
	. set $extract(%ZSHOWvbase("x"),%ZSHOWvbase("length")-1,%ZSHOWvbase("length"))=""
	. set %ZSHOWvbase("depth")=%ZSHOWvbase("depth")-1
	else  do sendmsg("m2",%ZSHOWvbase("x")) quit:$quit 0 quit
	if 1'<$data(@%ZSHOWvbase("x")) do sendmsg("m3",%ZSHOWvbase("x")) quit:$quit 0 quit
	set %ZSHOWvbase("cindex")=0								; this line & next do initialization
	set (%ZSHOWvbase("ret"),%ZSHOWvbase("m4"))=1
	for %ZSHOWvbase("index")=1:1 do  quit:'%ZSHOWvbase("data")!'%ZSHOWvbase("ret")	; main loop
	. set %ZSHOWvbase("data")=$data(@%ZSHOWvbase("x")@(%ZSHOWvbase("index")))
	. quit:'%ZSHOWvbase("data")
	. if 10=%ZSHOWvbase("data") do  quit							; not a legitimate node from ZWRITE
	. . set $extract(%ZSHOWvbase("x"),$length(%ZSHOWvbase("x")))=","_%ZSHOWvbase("index")_")"
	. . do sendmsg("m3",%ZSHOWvbase("x"))
	. set %ZSHOWvbase("piece")=@%ZSHOWvbase("x")@(%ZSHOWvbase("index"))
	. if "%ZSHOWvbase"=$extract(%ZSHOWvbase("piece"),1,$length("%ZSHOWvbase")) do  quit	; can't mess with our namespace
	. . do:%ZSHOWvbase("m4") sendmsg("m4")
	. . set %ZSHOWvbase("m4")=0
	. if "*"=$extract(%ZSHOWvbase("piece")) set @%ZSHOWvbase("piece") quit			; alias SET must use SET @
	. set %ZSHOWvbase("raw")=%ZSHOWvbase("piece"),(%ZSHOWvbase("key"),%ZSHOWvbase("value"))=""
	. if 11=%ZSHOWvbase("data") do  quit:'%ZSHOWvbase("ret")				; deal with continuation nodes
	. . for %ZSHOWvbase("cindex")=1:1 do  quit:1'=%ZSHOWvbase("cdata")!'%ZSHOWvbase("ret")	; loop thru continuation nodes
	. . . set %ZSHOWvbase("cdata")=$data(@%ZSHOWvbase("x")@(%ZSHOWvbase("index"),%ZSHOWvbase("cindex")))
	. . . quit:1'=%ZSHOWvbase("cdata")							; stop if end or non-conforming
	. . . set %ZSHOWvbase("piece")=@%ZSHOWvbase("x")@(%ZSHOWvbase("index"),%ZSHOWvbase("cindex"))
	. . . if 2**20>($zlength(%ZSHOWvbase("raw"))+$zlength(%ZSHOWvbase("piece"))) do  quit	; under max string length ?
	. . . . set %ZSHOWvbase("raw")=%ZSHOWvbase("raw")_%ZSHOWvbase("piece")			; if so, just concatenate
	. . . do rawtoval									; try shrinking prior accumulation
	. set %ZSHOWvbase("rlength")=$length(%ZSHOWvbase("raw"))
	. do:"*"=$extract(%ZSHOWvbase("raw"),%ZSHOWvbase("rlength"))				; remove training alias indicator
	. . set $extract(%ZSHOWvbase("raw"),%ZSHOWvbase("rlength")-2,%ZSHOWvbase("rlength"))=""; not relevant & causes trouble
	. set %ZSHOWvbase("cindex")=0								; flag this as the end
	. do rawtoval							; and prep
	. set:%ZSHOWvbase("ret") @%ZSHOWvbase("key")=%ZSHOWvbase("value")			; do actual assignment
	quit:$quit %ZSHOWvbase("ret") quit
rawtoval	; strips the key out of the raw accumulation and performs $ZWRITE(,1) compaction if needed
	do:""=%ZSHOWvbase("key")  quit:'%ZSHOWvbase("ret")					; find the key
	. set (%ZSHOWvbase("i"),%ZSHOWvbase("q"))=1,%ZSHOWvbase("key")=""			; find 1st equal-sign not in quotes
	. for  do  quit:%ZSHOWvbase("q")!'%ZSHOWvbase("i")
	. . set %ZSHOWvbase("c")=%ZSHOWvbase("i")
	. . set %ZSHOWvbase("i")=$find(%ZSHOWvbase("raw"),"=",%ZSHOWvbase("c"))
	. . if '%ZSHOWvbase("i") do  quit							; didn't find it
	. . . set $extract(%ZSHOWvbase("x"),$length(%ZSHOWvbase("x")))=","_%ZSHOWvbase("index")_")"
	. . . do sendmsg("m5",%ZSHOWvbase("x"))
	. . for %ZSHOWvbase("c")=%ZSHOWvbase("c"):1:%ZSHOWvbase("i")-2 do			; count quotes
	. . . set:""""=$extract(%ZSHOWvbase("raw"),%ZSHOWvbase("c")) %ZSHOWvbase("q")='%ZSHOWvbase("q")
	. set %ZSHOWvbase("key")=$extract(%ZSHOWvbase("raw"),1,%ZSHOWvbase("i")-2)
	. if 8192<$zlength(%ZSHOWvbase("key")) do  quit:'%ZSHOWvbase("ret")			; can we indirect the key?
	. . set %ZSHOWvbase("key")=$zwrite(%ZSHOWvbase("key"),1)				; if not attempt to shrink it
	. . quit:$length(%ZSHOWvbase("key"))							; it worked!
	. . set $extract(%ZSHOWvbase("x"),$length(%ZSHOWvbase("x")))=","_%ZSHOWvbase("index")_")"
	. . do sendmsg("m6",%ZSHOWvbase("x"))
	. set %ZSHOWvbase("rlength")=$length(%ZSHOWvbase("raw"))
	. set %ZSHOWvbase("raw")=$extract(%ZSHOWvbase("raw"),%ZSHOWvbase("i"),%ZSHOWvbase("rlength"))	;strip off key
	if '%ZSHOWvbase("cindex") set %ZSHOWvbase("rawval")=$zwrite(%ZSHOWvbase("raw"),1)	; no more to add on
	else  do  quit:'%ZSHOWvbase("ret")							; need to fit more in
	. ; lines below are an imperfect heuristic because of possible quoting but quicker than parsing the whole value
	. set %ZSHOWvbase("f")=0
	. set %ZSHOWvbase("rlength")=$length(%ZSHOWvbase("raw"))
	. for %ZSHOWvbase("i")=%ZSHOWvbase("rlength")-1:-1:0 do  quit:%ZSHOWvbase("f")	; start 1 back for room to add quote
	. . set %ZSHOWvbase("c")=$extract(%ZSHOWvbase("raw"),%ZSHOWvbase("i"))	; 		; do alphas 1st as more likely
	. . if (%ZSHOWvbase("c")?1AP&("$C(,)_"""'[%ZSHOWvbase("c"))),$increment(%ZSHOWvbase("f")) do; ?AP not in $C(,)_" safe split
	. . . set %ZSHOWvbase("rawval")=$zwrite($extract(%ZSHOWvbase("raw"),1,%ZSHOWvbase("i"))_"""",1)	; shrink
	. . . set %ZSHOWvbase("raw")=$extract(%ZSHOWvbase("raw"),%ZSHOWvbase("i")+1,%ZSHOWvbase("rlength"))_%ZSHOWvbase("piece")
	. . . if """_"=$extract(%ZSHOWvbase("raw"),1,2) set $extract(%ZSHOWvbase("raw"),1,2)=""	;  "_ needs removal
	. . . else  set %ZSHOWvbase("raw")=""""_%ZSHOWvbase("raw")				; otherwise start remaining with "
	. do:'%ZSHOWvbase("f")								; no easy split; try to break up a $C()
	. . for %ZSHOWvbase("i")=%ZSHOWvbase("rlength"):-1:0 do  quit:%ZSHOWvbase("f")!'%ZSHOWvbase("ret")
	. . . set %ZSHOWvbase("c")=$extract(%ZSHOWvbase("raw"),%ZSHOWvbase("i"))		; this loop is less than perfect
	. . . do:","=%ZSHOWvbase("c")&($extract(%ZSHOWvbase("raw"),%ZSHOWvbase("i")-1)?1N)	; argument delimiter in $C()
	. . . . if $increment(%ZSHOWvbase("f"))
	. . . . set ($extract(%ZSHOWvbase("raw"),%ZSHOWvbase("i")),%ZSHOWvbase("c"))=")"	; replace comma by closing function
	. . if ")"=%ZSHOWvbase("c"),$extract(%ZSHOWvbase("raw"),%ZSHOWvbase("i")-1)?1N,$increment(%ZSHOWvbase("f")) do  quit
	. . . set %ZSHOWvbase("rawval")=$zwrite($extract(%ZSHOWvbase("raw"),1,%ZSHOWvbase("i")),1)
	. . . set %ZSHOWvbase("raw")=$extract(%ZSHOWvbase("raw"),%ZSHOWvbase("i")+3-%ZSHOWvbase("f"),%ZSHOWvbase("rlength"))
	. . . set %ZSHOWvbase("raw")=%ZSHOWvbase("raw")_%ZSHOWvbase("piece")
	. . . set:1<%ZSHOWvbase("f") %ZSHOWvbase("raw")="$C("_%ZSHOWvbase("raw")		; bigger "f" so restart $C()
	. . if '%ZSHOWvbase("f") do								; maybe it's all $C(,)_ "s
	. . . for %ZSHOWvbase("i")=%ZSHOWvbase("rlength"):-1:0 do  quit:%ZSHOWvbase("f")
	. . . . set %ZSHOWvbase("c")=$extract(%ZSHOWvbase("raw"),%ZSHOWvbase("i"))
	. . . . if "$C(,)_"""[%ZSHOWvbase("c"),$increment(%ZSHOWvbase("f")) do
	. . . . . set %ZSHOWvbase("rawval")=$zwrite($extract(%ZSHOWvbase("raw"),1,%ZSHOWvbase("i"))_"""",1)	; shrink
	. . . . . set %ZSHOWvbase("raw")=$extract(%ZSHOWvbase("raw"),%ZSHOWvbase("i")+1,%ZSHOWvbase("rlength"))_%ZSHOWvbase("piece")
	. . . . . if """"=$extract(%ZSHOWvbase("raw")) set $extract(%ZSHOWvbase("raw"),1,2)=""	; " means "_ needs removal
	. . . . . else  set %ZSHOWvbase("raw")=""""_%ZSHOWvbase("raw")				; otherwise start remaining with "
	. . if '%ZSHOWvbase("f") do								; no luck at all
	. . . set $extract(%ZSHOWvbase("x"),$length(%ZSHOWvbase("x")))=","_%ZSHOWvbase("index")_")"
	. . . do sendmsg("m6",%ZSHOWvbase("x"))
	do:""""""'=%ZSHOWvbase("raw")						; empty in produces empty out so skip error check
	. if ""=%ZSHOWvbase("rawval")!(2**20'>($zlength(%ZSHOWvbase("rawval"))+$zlength(%ZSHOWvbase("value")))) do
	. . set $extract(%ZSHOWvbase("x"),$length(%ZSHOWvbase("x")))=","_%ZSHOWvbase("index")_")"
	. . do sendmsg("m6",%ZSHOWvbase("x"))
	set %ZSHOWvbase("value")=%ZSHOWvbase("value")_%ZSHOWvbase("rawval")
	quit
sendmsg(x,y)				; message thing - could be turned into $zstatus settor if there's a need for set $ecode
	new i,io,m
	set m=$text(@x)
	set %ZSHOWvbase("ret")=$piece(m,";",2)							; maintain "ret" to exit or not
	set m=$piece(m,";",3,99)
	set:""=m m=x_" is not a valid message for sendmsg^"_$text(+0)_" or the source module is unavailable"
	set i=$find(m,"~")
	set:i $extract(m,i-1)=$get(y)
	set io=$io
	use $principal
	write !,m,!
	use io
	quit
m0	;0;^%ZSHOWVTOLCL won't run if ""'=$ecode
m1	;0;~ is not a global reference
m2	;0;~ does not match base for ZSHOW "V" format
m3	;0;~ does not contain ZSHOW "V" data
m4	;1;Cannot restore %ZSHOWvbase
m5	;0;Could not extract a valid key from ~
m6	;0;Could not work ~ into a value within current processing limits
