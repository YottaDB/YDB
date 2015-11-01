;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 1988, 2003 Sanchez Computer Associates, Inc.	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%RCE	;GT.M %RCE utility - change every occurrence of a string in one or more routines
	;invoke ^%RCE to get interaction
	;invoke CALL^%RCE with %ZF - string to find, %ZN - new string, %ZR - routine array or name,
	;			%ZD an optional device to receive a trail
	;
	n cnt1,cnt2,cnt3,fnd,h,i,o,out,outd,r,tf,x,xn,%ZC,%ZD,%ZF,%ZN,%ZR
	i '$d(%zdebug) n $et s $et="zg "_$zl_":ERR^%RCE" u $p:(ctrap=$c(3):exc="zg "_$zl_":LOOP^%RCE")
	d init,MAIN
	u $p:(ctrap="":exc="")
	q
CALL	i '$l($g(%ZF)) q
	n zc,zd s zc=$G(%ZC),zd=$G(%ZD)
	n cnt1,cnt2,cnt3,fnd,h,i,o,out,outd,r,tf,x,xn,%ZD,%ZC,lzd n:'$d(%ZN) %ZN
	s %ZC=zc,%ZD=zd
	s %ZD=$g(%ZD),%ZN=$g(%ZN),(cnt1,cnt2,cnt3,out,outd)=0,tf=$j_"rce.tmp",lzd=$l(%ZD) s:'lzd %ZD=$P
	s:%ZC outd=$l(%ZD) s:'outd %ZD=$p s:'lzd %ZD=$P s %ZC=1
	i $d(%ZR)<10 d CALL^%RSEL
	d work
	q
init	s %ZC=1,(cnt1,cnt2,cnt3)=0,out=1,tf=$j_"rce.tmp"
	w !,"Routine Change Every occurrence",!
	q
MAIN	s %ZR="" d CALL^%RSEL
	i %ZR=0 w !,"No routines selected" q
	w !,$s(%ZC:"Old",1:"Find")," string: " r %ZF
	i '$l(%ZF) w !,"No search string to find - no search performed",! q
	i %ZF?.E1C.E w !,"The find string contains control characters"
	i %ZC r !,"New string: ",%ZN
	i %ZC,%ZN?.E1C.E w !,"The New string contains control characters"
	w !,$s(%ZC:"Replace",1:"Find")," all occurrences of:",!,">",%ZF,"<",! i %ZC w "With: ",!,">",%ZN,"<",!
	i %ZC f  r !,"Show changed lines <Yes>?: ",x,! q:$e(x)'="?"  d help
	i %ZC,$l(x) q:"\QUIT"[("\"_$tr(x,"quit","QUIT"))  s outd=$e("NO",1,$l(x))'=$e($tr(x,"no","NO"),1,2)
	e  s outd=1
	i outd f  d  q:$l(%ZD)
	. r !,"Output device: <terminal>: ",%ZD,!
	. i '$l(%ZD) s %ZD=$p q
	. i %ZD="^" q
	. i %ZD="?" d  q
	 . . w !!,"Select the device you want for output"
	 . . w !,"If you wish to exit enter a carat (^)",!
	 . . s %ZD=""
	. i $zparse(%ZD)="" w "  no such device" s %ZD="" q
	. o %ZD:(newversion:block=2048:record=2044:exception="g noopen"):0
	. i '$t  w !,%ZD," is not available" s %ZD="" q
	. q
noopen	. w !,$p($ZS,",",2,999),! c %ZD s %ZD=""
	i '$D(%ZD) s %ZD=""
	q:%ZD="^"
	w !
	d work
	q
work	s %ZR="",r=$zsearch("__")
	i outd,%ZD'=$p u %ZD w $zd($h,"DD-MON-YEAR 24:60:SS"),!
	i  w "Routine ",$s(%ZC:"Change",1:"Search for")," Every occurrence of:",!,">",%ZF,"<",! i %ZC w "To:",!,">",%ZN,"<",!
	i '%ZC d
	. s gtmvt=$$GTMVT^%GSE
	. i gtmvt s sx=$c(27)_"[7m"_%ZF_$c(27)_"[0m"
	. e  s sx=%ZF,flen=$l(%ZF),tics=$tr($j("",flen)," ","^")
	f  s %ZR=$o(%ZR(%ZR)) q:'$l(%ZR)  d scan
	q:'out
	u %ZD w !!,"Total of ",cnt1," routine",$s(cnt1=1:"",1:"s")," parsed.",!
	w cnt2," occurrence",$s(cnt2=1:" ",1:"s "),$s(%ZC:"changed",1:"found")," in ",cnt3," routine",$s(cnt3=1:".",1:"s."),!
	c %ZD
	q
scan	s r=%ZR(%ZR)_$tr($e(%ZR),"%","_")_$e(%ZR,2,9999)_".m",o=$zsearch(r),fnd=0
	u $p i out,%ZD'=$p!'outd w:$x>70 ! w %ZR,?$x\10+1*10
	i outd u %ZD w !!,r
	o:%ZC tf:(newversion:exception="s fnd=0 g reof") o o:(readonly:record=2048:rewind:exception="g rnoopen")
	u o:exception="g reof"
	s cnt1=cnt1+1
	f  u o r x s h=$l(x,%ZF) d
	. i h=1 d:%ZC  q
	. . u tf w x,!
	. s fnd=fnd+h-1
	. i %ZC d  q
	. . i outd u %ZD w !,"Was: " w x
	. . s xn=""
	. . f i=1:1:h-1 s xn=xn_$p(x,%ZF,i)_%ZN
	. . s xn=xn_$p(x,%ZF,h)
	. . i outd w !,"Now: ",xn
	. . u tf w xn,! q
	. u %ZD w ! s rl=""
	. f i=1:1:h-1 s p=$tr($p(x,%ZF,i),$c(9)," ") w p,sx i 'gtmvt s rl=rl_$j(tics,$l(p)+flen)
	. w $p(x,%ZF,h)
	. i 'gtmvt w !,rl
	q
reof	i fnd s cnt2=cnt2+fnd,cnt3=cnt3+1 i %ZC c:$zver'["VMS" o:(DELETE) c tf:(RENAME=r)
	e  c o i %ZC c tf:(DELETE)
	; warning - fall-through
rnoopen	i $zs'["EOF" w !,$p($zs,",",2,999),!
	q
help	i "Dd"[$e(x,2),$l(x)=2 d cur q
	i %ZC w !,"Answer No to this prompt if you do not wish a trail of the changes"
	w !,"Enter Q to exit",!
	w !,"?D for the current routine selection"
	q
cur	w ! s r=""
	f  s r=$o(%ZR(r)) q:'$l(r)  w:$x>70 ! w r,?$x\10+1*10
	q
ERR	i $d(tf) c tf:(DELETE)
	i $d(o) c o
	i $d(%ZD),%ZD'=$p c %ZD
	u $p w !,$p($ZS,",",2,999),!
	u $p:(ctrap="":exception="")
	s $ec=""
	q
LOOP	i $d(tf) c tf:delete
	i $d(o) c o
	i $d(%ZD),%ZD'=$p c %ZD
	d MAIN
	u $p:(ctrap="":exception="")
	q
