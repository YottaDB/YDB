;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 2001 Sanchez Computer Associates, Inc.	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	s warning=0,success=1,error=2,info=3,fatal=4,severe=4
	r "message file> ",fn,!
	s in=$zparse(fn,"","",".msg"),out=$zparse(fn,"NAME"),out=$zparse(out,"","[]",".c")
	s fn=$zparse(fn,"NAME")
	s fn=$tr(fn,"ABCDEFGHIJKLMNOPQRSTUVWXYZ","abcdefghijklmnopqrstuvwxyz")
	s cnt=0
	o in:readonly o out:newv
	u in f  r msg q:msg?.E1".FACILITY".E
	d fac
	f  u in q:$zeof  r msg i msg?1U.E d msg
	u out w "};",!,!
	f i=1:1:cnt w "LITDEF",$c(9),"int ",prefix,outmsg(i)," = ",outmsg(i,"code"),";",!
	w !,"LITDEF",$c(9),"err_ctl "_fn_"_ctl = {",!
	w $c(9),facnum,",",!
	w $c(9),""""_facility_""",",!
	w $c(9),"&"_fn_"[0],",!
	w $c(9),cnt_"};",!
	q
msg	s back=msg
	f i=1:1 q:($a(msg,i)=9)!($a(msg,i)=32)
	s cnt=cnt+1
	i cnt>4095 u 0 w in_" format error",!,back b
	s outmsg(cnt)=$e(msg,1,i-1)
	s msg=$e(msg,i,999)
	f i=1:1 q:$a(msg,i)=60
	s msg=$e(msg,i+1,999)
	s text=""""
	f i=1:1 q:$a(msg,i)=62  s:$a(msg,i)=34 text=text_"\" s text=text_$e(msg,i)
	s text=text_""""
	s msg=$e(msg,i+1,999)
	f i=1:1 q:$a(msg,i)=47
	s msg=$e(msg,i+1,999)
	f i=1:1 q:($a(msg,i)=47)!($a(msg,i)=32)
	s severity=$e(msg,1,i-1) f j=$l(severity):-1:1 q:($a(severity,j)'=32)&($a(severity,j)'=9)  s severity=$e(severity,1,j-1)
	i $a(msg,i)=32 f i=i:1 q:$a(msg,i)=47
	s msg=$e(msg,i+1,999)
	i msg'?1"fao=".e u 0 w in_" format error",!,back b
	s msg=$e(msg,5,999)
	s fao=msg
	s severity=$tr(severity,"ABCDEFGHIJKLMNOPQRSTUVWXYZ","abcdefghijklmnopqrstuvwxyz")
	s outmsg(cnt,"code")=(facnum+2048)*65536+((cnt+4096)*8)+@severity
	u out w $c(9),"""",outmsg(cnt),""", ",text,", ",fao,",",!
	q
fac	f i=1:1 q:($a(msg,i)'=9)&($a(msg,i)'=32)
	s msg=$e(msg,i,999)
	i msg'?1".FACILITY".E u 0 w "MESSAGE file format error",! b
	s msg=$e(msg,10,999)
	f i=1:1 q:($a(msg,i)'=9)&($a(msg,i)'=32)
	s msg=$e(msg,i,999)
	f i=1:1 q:($a(msg,i)=9)!($a(msg,i)=32)!($a(msg,i)=44)
	s facility=$e(msg,1,i-1)
	i facility>2047 u 0 w in_" format error",!,back b
	s msg=$e(msg,i+1,999)
	f i=1:1 q:($a(msg,i)'=9)&($a(msg,i)'=32)&($a(msg,i)'=44)
	s msg=$e(msg,i,999)
	f i=1:1 q:($a(msg,i)=9)!($a(msg,i)=32)!($a(msg,i)=47)
	s facnum=$e(msg,1,i-1)
	s msg=$e(msg,i+1,999)
	f i=1:1 q:($a(msg,i)'=9)&($a(msg,i)'=32)&($a(msg,i)'=47)
	s msg=$e(msg,i,999)
	i msg'?1"PREFIX".E u 0 w "MESSAGE file format error",! b
	s msg=$e(msg,7,999)
	f i=1:1 q:($a(msg,i)'=9)&($a(msg,i)'=32)&($a(msg,i)'=61)
	s msg=$e(msg,i,999)
	f i=1:1:$l(msg) q:($a(msg,i)=9)!($a(msg,i)=32)
	s prefix=$e(msg,1,i)
	u out w "#include ""mdef.h""",!
	w "#include ""merror.h""",!,!
	w "LITDEF"_$c(9)_"err_msg "_fn_"[] = {",!
	q
