;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
map:	;create maps for put and show, names for get and show
PUTMAKE
	k lexnams n t1
	d SHOWMAKE
	s s1=$tr($j("",SIZEOF("mident"))," ",$c(255)),t1=$tr($j("",SIZEOF("mident"))," ",$c(0))
	f  s s2=s1,s1=$o(map(s1),-1),map(s2_$e(t1,$l(s2)+1,SIZEOF("mident")))=map(s1) q:s1="$"  k map(s1)
	s map("#)"_$e(t1,3,SIZEOF("mident")))=map("#)"),map("%"_$e(t1,2,SIZEOF("mident")))=map("$")
	f s2="#","#)","$" k map(s2)
 	q
;----------------------------------------------------------------------------------------------------------------------------------
SHOWMAKE
	n lexnams s s=""
	f  s s=$o(nams(s)) q:'$l(s)  d lexins(s)
	s map("$")=nams("*"),map("#")=nams("#")
	s i=1
	f  s i=$o(lexnams(i)) q:'$l(i)  d showstar(i)
	s s=""
	f  s s=$o(lexnams(0,s),-1) q:'$l(s)  d pointins(s,lexnams(0,s))
	s s1=$o(map(""),-1)
	f  s s2=s1,s1=$o(map(s1),-1) q:s2="$"  i map(s1)=map(s2) k map(s2)
	q
;----------------------------------------------------------------------------------------------------------------------------------
SHOWNAM
	n lexnams,t1,map
	d SHOWMAKE
	s s1=$tr($j("",SIZEOF("mident"))," ",$c(255)),t1=$tr($j("",SIZEOF("mident"))," ",$c(0))
	f  s s2=s1,s1=$o(map(s1),-1),map(s2)=map(s1) q:s1="$"  k map(s1)
	k map("#") i '$$MAP2NAM(.map) zm gdeerr("GDECHECK")\2*2
 	q
;----------------------------------------------------------------------------------------------------------------------------------
MAP2NAM(list)
	n ar,l,s,x,xl,xn,xp,xr,xs,xt,xv
	s x=$o(list("")) q:x'="#)" 0
	s x=$o(list(x))
	i x="%" s list("$")=list("%") k list("%")
	e  q:x'="$" 0
	s s=$tr($j("",SIZEOF("mident"))," ",$c(255)),x=$o(list(""),-1) q:x'=s 0
	k ar,nams s nams=0,ar(0,s)="",x=$o(list(x),-1)
	i x'="$",list(x)'=list(s) s xp="z" f  q:xp=""  s xn=xp_"*",(nams(xn),ar(1,xn))="",xp=$$lexprev(xp)
	f  q:x="$"  d  s x=$o(list(x),-1)
	. i $d(list(x_")")) q
	. s xl=$l(x)
	. i $e(x,xl)=")" s xn=$e(x,1,xl-1),(nams(xn))="" q
	. s xp=x,(ip,in)=0,star=$s(xl<SIZEOF("mident"):"*",1:"")
	. f  s xp=$$lexprev(xp),l=$l(xp) q:'l  s ip=ip+1 q:l=xl&$d(list(xp))
	. i ip'=1 s xn=x f  s xn=$$lexnext(xn),l=$l(xn) q:'l  s in=in+1 q:l'>xl&($d(list(xn))!$d(list(xn_")")))
	. i ip,ip'>in!('in&$l($tr(x,"z"))) s xp=x f ip=1:1:ip s xp=$$lexprev(xp),xn=xp_star,(nams(xn),ar(xl,xn))=""
	. e  s:'in&'$l($tr(x,"z")) in=1 s xn=x f in=1:1:in d  q:$d(nams(xn))!$d(list(xn_")"))
	. . s xp=xn_star,(nams(xp),ar($l(xn),xp))="",xn=$$lexnext(xn)
	s x="",ok=1
	f xl=1:1:SIZEOF("mident") d  q:'ok
	. f  s x=$o(ar(xl,x)) q:x=""  d  q:'ok
	. . s xt=$o(list(x))
	. . i '$l(xt) s ok=0 q
	. . s xv=list(xt)
	. . i '$l(xv) s ok=0 q
	. . s xn=$$lexnext($e(x,1,xl))
	. . f l=xl-1:-1:0 s (xp,xs)=$e(x,1,l) d  q:$l(xp)
	. . . f  s xp=$o(ar(l,xp)) q:'$l(xp)  s xr=$s('l:"$",$e(xp,1,l)'=xs:xt,1:$o(list(xp))) i xs']xr!'$l(xr)!'l q
	. . . s:'$l(xr) xp="" q:'$l(xp)
	. . . i xt=xr&(xl>1) s xp=" " q
	. . . s xp=list(xr)
	. . . i '$l(xp) s ok=0 q
	. . . i xv=xp k nams(x)
	i 'ok q 0
	f  s x=$o(nams(x)) q:x=""  s xn=$o(list(x)),nams(x)=list(xn),nams=nams+1
	s nams("#")=list("#)"),nams("*")=list("$"),nams=nams+2
	q 1
;----------------------------------------------------------------------------------------------------------------------------------
lexins:(s)
	n x,l
	i s["*" s l=$l(s),x=$e(s,1,l),lexnams(l,x)=nams(s)
	e  s lexnams(0,s)=nams(s)
	q
showstar:(i)
	s j=""
	f  s j=$o(lexnams(i,j),-1) q:'$l(j)  d starins($e(j,1,$l(j)-1),lexnams(i,j))
	q
starins:(s,reg)
	n next
	s next=$$lexnext(s)
	i $l(next),'$d(map(next)) s map(next)=map($o(map(next),-1))
	s map(s)=reg
	q
pointins:(s,reg)
	n next
	s next=$$alphnext(s)
	i $l(next),'$d(map(next)) s map(next)=map($o(map(next),-1))
	s map(s)=reg
	q
alphnext:(s)
	i $l(s)=MAXNAMLN q $$lexnext(s)
	e  q s_")"
	;
lexnext:(s)
	n len,last,succ
	s len=$l(s),last=$e(s,len)
	i last="z" f  s len=len-1,last=$e(s,len) q:last'="z"!'len
	i 'len q ""
	s s=$e(s,1,len-1),succ=$c($a(last)+1)
	i succ?1AN q s_succ
	i "A"]succ q s_"A"
	i "a"]succ q s_"a"
	q ""
lexprev:(s)
	n len,last,prio
	s len=$l(s),last=$e(s,len)
	s s=$e(s,1,len-1),prio=$c($a(last)-1)
	i prio?1AN q s_prio
	i prio]"Z" q s_"Z"
	i prio]"9" q:len=1 "%" q s_"9"
	q ""
