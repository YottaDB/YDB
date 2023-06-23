;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
; Copyright (c) 1987-2017 Fidelity National Information		;
; Services, Inc. and/or its subsidiaries. All rights reserved.	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%TI	; GT.M %TI utility - convert time to $H format
	; invoke ^%TI to enter time interactively; returns %TN
	; invoke INT^%TI with %TS to convert a previously captured time
	; $$FUNC^%TI() with an appropriate argument returns time
	; formats accepted include:
	;	[h]h mm [a/p[m]]
	;	hhmm interpreted as 24 hr (military) time
	;	unique suffixes of midni(ght/te), and noon
	;	any punctuation character(s) are accepted as delimiters
	new %TS,d
	set d("io")=$io
	if '$data(%zdebug) new $etrap set $etrap="zgoto "_$zlevel_":err^"_$text(+0) do
	. zshow "d":d									; save original $p settings
	. set d=$piece($piece(d("D",1),"CTRA=",2)," ")
	. set:d?1."""" d=""
	. set d("use")="$principal:(ctrap="""_d_""":exception=""",d=$piece(d("D",1),"EXCE=",2),d=$zwrite($extract(d,2,$length(d)-1))
	. set:d?1."""" d=""
	. set d("use")=d("use")_d_""":"_$select($find(d("D",1),"NOCENE"):"nocenable",1:"cenable")_")"
	. use $principal:(ctrap=$char(3,4):exception=d:nocenable)
	for  read !,"Time: ",%TS set %TN=$$FUNC(%TS) quit:""'=%TN  write "  - invalid time"
	use:$data(d("use")) @d("use")
	use:$data(d("io")) d("io")
	quit
INT	set %TN=$$FUNC($get(%TS))
	quit
FUNC(ts)
	quit:(""=$get(ts)) $piece($horolog,",",2)
	if '$data(%zdebug) new $etrap set $etrap="zgoto "_$zlevel_":err^"_$text(+0)
	new apm,cp,dir,hr,ilen,min,ocp,tp,tok,dh
	set dh="",min=0
	set ilen=$length(ts)+1,ocp=1
	do advance quit:(dir?1C) ""
	quit:(tok?1A.E) $select("\NOON"[("\"_tok):43200,"\MIDNIGHT\MIDNITE"[("\"_tok):0,1:"")
	if tok?4N set min=$extract(tok,3,4),hr=$extract(tok,1,2) quit:(24<hr)!(59<min) ""
	else  if (tok?1.N) quit:24<tok "" do  if dir'?1A do advance quit:(dir?1C) "" if tok?1.N quit:(59<tok) "" set min=tok
	. set hr=tok,min=0
	if dir?1A quit:12<hr "" do advance quit:(dir?1C) "" do  quit:(0>apm) ""
	. set apm=$find("\AM\PM\MI\NO","\"_$extract(tok,1,2))-3\3
	. set:((1<apm)&min) apm=-1
	. if 0<=apm set:(12=hr)&(apm<2) hr=apm*hr set hr=$select((0=apm):hr,(1=apm)&(12'=hr):12+hr,2=apm:0,3:12)
	quit $select((ocp+1<ilen):"",1:((hr*60)+min)*60)
	;
advance	for cp=ocp:1:ilen quit:$extract(ts,cp)?1AN
	set dir=$extract(ts,cp)
	if dir?1A for tp=cp+1:1:ilen quit:$extract(ts,tp)'?1A
	else  if dir?1N for tp=cp+1:1:ilen quit:$extract(ts,tp)'?1N
	if  set tok=$extract(ts,cp,tp-1) set:(dir?1A) tok=$zconvert(dir,"U")
	else  set tok="",tp=ocp
	for cp=tp:1:ilen set dir=$extract(ts,cp) quit:dir'?1P
	set dir=$zconvert(dir,"U"),ocp=cp
	quit
err	write !,$piece($zstatus,",",2,99),!
	use:$data(d("use")) @d("use")
	use:$data(d("io")) d("io")
	set $ecode=""
	quit
