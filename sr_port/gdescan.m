;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 2001, 2010 Fidelity Information Services, Inc	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
gdescan: ;scanner used by gdeparse
GETTOK
	n c
	s token=ntoken,toktype=ntoktype
	f cp=cp:1 s c=$e(comline,cp) q:(c'=" ")&(c'=TAB)
	i $c(10,13)[c,cp=$l(comline) s c=""
	s ntoktype=$s(c?1A:"TKIDENT",c?1N:"TKNUMLIT",c="":"TKEOL",$d(tokens(c)):tokens(c),1:"TKOTHER")
	d @ntoktype
	q
shotoks: ; for debugging only
	w !,"  toktype: ",toktype,?24," token: '",token,"'"
	w ?48," ntoktype: ",ntoktype,?72,"ntoken: '",ntoken,"'"
	q
TKIDENT
	n i
	f i=1:1 s c=$e(comline,cp+i) q:(c'?1A)&(c'="_")
	s ntoken=$e(comline,cp,cp+i-1),cp=cp+i
	q
TKNUMLIT
	n i
	f i=1:1 q:$e(comline,cp+i)'?1N
	s ntoken=$e(comline,cp,cp+i-1),cp=cp+i
	q
TKSTRLIT
	n i
	f i=1:1:$l(comline)-cp q:$e(comline,cp+i)=""""
	s ntoken=$e(comline,cp,cp+i),cp=cp+i+1
	q
TKAMPER
TKASTER
TKCOLON
TKCOMMA
TKDASH ; see below for more UNIXy alternative
TKDOLLAR
TKEQUAL
TKLANGLE
TKLBRACK
TKLPAREN
TKPCT
TKPERIOD
TKRANGLE
TKRBRACK
TKRPAREN
TKSCOLON
TKSLASH
TKUSCORE
	s ntoken=c,cp=cp+1
	q
TKEXCLAM
	s ntoktype="TKEOL"
	s ntoken=""
	s cp=$l(comline)
	q
;TKDASH - more UNIXy handling disabled for compatibility with other utilities
	s ntoken=c,cp=cp+1
	i sep="TKDASH",$e(comline,cp)?1A s c=$e(comline,cp-2) i c=" "!(c=TAB) q
	zm gdeerr("ILLCHAR"):"-"
	q
TKEOL
	s ntoken=""
	q
TKOTHER
	zm gdeerr("ILLCHAR"):c
