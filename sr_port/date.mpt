;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 1987, 2003 Sanchez Computer Associates, Inc.	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%DATE	;GT.M %DATE utility - returns $H format date in %DN
	;invoke at INT with %DS to by pass interactive prompt
	;formats accepted include:
	;	days as 1 or 2 digits
	;	months as 1 or 2 digits or as unique alpha prefixes
	;	years as 2 or 4 digits
	;	"T" or "t" for today with an optional +/- offset
	;	except for the +/- offset any non alphameric character(s)
	;		 are accepted as delimiters
	;	numeric months must precede days
	;	alpha months may precede or follow days
	;	a missing year is defaulted to this year
	;	null input is defaulted to today
	;
	n %DS
	f  r !,"Date: ",%DS s %DN=$$FUNC(%DS) q:%DN'=""  w "  - invalid date"
	q
INT	s %DN=$$FUNC($g(%DS))
	q
FUNC(dt)
	n cp,dd,dir,ilen,mm,tok,tp,yy,dh,zd
	i $g(dt)="" q +$H
	s ilen=$l(dt)+1,cp=1 d advance
	i $e("TODAY",1,$l(tok))=$tr(tok,"today","TODAY") q $$incr(+$H)
	i $e("TOMORROW",1,$l(tok))=$tr(tok,"tomrw","TOMRW") q $$incr($H+1)
	i $e("YESTERDAY",1,$l(tok))=$tr(tok,"yestrday","YESTRDAY") q $$incr($H-1)
	i dir?1A s mm=$$amon(tok) q:mm=0 "" d advance s dd=tok q $$date
	i tok<1 q ""
	s mm=tok d advance
	i dir?1A s dd=mm,mm=$$amon(tok) q:mm=0 "" q $$date
	i mm<13 s dd=tok q $$date
	q ""
	;
advance	f cp=cp:1:ilen q:$e(dt,cp)?1AN
	s dir=$e(dt,cp)
	i dir?1A f tp=cp+1:1:ilen q:$e(dt,tp)'?1A
	e  i dir?1N f tp=cp+1:1:ilen q:$e(dt,tp)'?1N
	e  s tok="" q
	s tok=$e(dt,cp,tp-1)
	s cp=tp
	q
incr(h)
	f cp=cp:1:ilen q:"+-"[$e(dt,cp)
	i cp'=ilen s dd=$e(dt,cp) d advance i dir?.1N,cp=ilen q h+(dd_tok)
	q h
	;
amon(mm)
	s mm=$tr(mm,"abcdefghijklmnopqrstuvwxyz","ABCDEFGHIJKLMNOPQRSTUVWXYZ")
	i $l(mm)<3,"AJM"[tok,"JAPAU"'[mm q 0
	n mon
	s mon="\JANUARY  \FEBRUARY \MARCH    \APRIL    \MAY      \JUNE     \JULY     \AUGUST   \SEPTEMBER\OCTOBER  \NOVEMBER \DECEMBER"
	q $f(mon,("\"_mm))+8\10
	;
date()
	i dd<1 q ""
	d advance
	i dir'?.1N q ""
	s zd=$ZDATEFORM
	s yy=tok
	i yy="" s yy=$zd($h,"YEAR")
	i cp'=ilen q ""
	i $l(yy)<3 d
	. s dh=$H
	. s yy=yy+(100*$S('zd:19,(zd>1840)&($L(zd)=4):($E(zd,1,2)+$S($E(zd,3,4)'>yy:0,1:1)),1:$E($ZDATE(dh,"YEAR"),1,2)))
	;                  20th           rolling                                current century
	i dd>$s(+mm'=2:$e(303232332323,mm)+28,yy#4:28,yy#100:29,yy#400:28,1:29) q ""
	n cc,dat
	s dat=yy-1841,mm=mm-1,cc=1
	i dat<0 s dd=dd-1,cc=-1
	s dat=dat\4*1461+(dat#4-$s(dat'<0:0,1:4)*365)+(mm*30)+$e(10112234455,mm)+dd-(yy-1800\100-(yy-1600\400))
	i yy#4,mm>1 s dat=dat-cc
	i yy#100=0,mm<2,yy#400 s dat=dat+cc
	q dat
