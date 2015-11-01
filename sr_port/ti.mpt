;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 1987,2001 Sanchez Computer Associates, Inc.	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%TI	;GT.M %TI utility - convert time to $H format
	;invoke ^%TI to enter time interactively; returns %TN
	;invoke INT^%TI with %TS to convert a previously captured time
	;formats accepted include:
	;	[h]h mm [a/p[m]]
	;	hhmm is interpreted as 24 hr (military) time
	;	unique prefixes of midni(ght/te), and noon
	;	any non-alphameric character(s) are accepted as delimiters
	n %TS
	f  r !,"Time: ",%TS s %TN=$$FUNC(%TS) q:%TN'=""  w "  - invalid time"
	q
INT	s %TN=$$FUNC($g(%TS))
	q
FUNC(ts)
	n apm,cp,dir,hr,ilen,min,tp,tok,dh
	s dh="",min=0
	i $g(ts)="" q $p($H,",",2)
	s ilen=$l(ts)+1,cp=1 d advance
	i dir?1A q $s("\NOON"[tok:43200,"\MIDNIGHT\MIDNITE"[tok:0,1:"")
	i tok?4N,cp=ilen s min=$e(tok,3,4),hr=$e(tok,1,2) q:hr>24!(min>59) "" q hr*60+min*60
	i tok'?1.2N!(tok>12) q ""
	s hr=$s(tok=12:0,1:tok)
	i $e(ts,cp,cp+2)?1":"2N d advance q:tok>59 "" s min=tok
	d advance
	i dir'?1A!(cp'=ilen) q ""
	s apm=$f("\AM\PM",tok)-3\3
	i apm'<0 q apm*12+hr*60+min*60
	q $s(hr:"",min:"","\NOON"[tok:43200,"\MIDNIGHT\MIDNITE"[tok:0,1:"")
	;
advance	f cp=cp:1:ilen q:$e(ts,cp)?1AN
	s dir=$e(ts,cp)
	i dir?1A f tp=cp+1:1:ilen q:$e(ts,tp)'?1A
	e  i dir?1N f tp=cp+1:1:ilen q:$e(ts,tp)'?1N
	e  s tok="" q
	s tok=$e(ts,cp,tp-1)
	i dir?1A s tok="\"_$tr(tok,"abcdefghijklmnopqrstuvwxyz","ABCDEFGHIJKLMNOPQRSTUVWXYZ")
	s cp=tp
	q
