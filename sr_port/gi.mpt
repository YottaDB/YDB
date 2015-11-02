;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 1991, 2006 Fidelity Information Services, Inc	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%GI	;service@greystone.com %GO;19920722 21:35;global input
	;Load globals into database
	;Possible enhancements:
	;selection and/or exclusion by key list, range and/or wildcard
	;optional confirmation by global name
	;callable entry point
	;
	w !,"Global Input Utility",!
	i '$d(%zdebug) n $et s $et="zg "_$zl_":ERR^%GI" u $p:(ctrap=$c(3):exc="zg "_$zl_":EXIT^%GI")
	n d,g,n,sav,x,y,%ZD,fmt,ctls
	s ctls="" f d=1:1:31,127 s ctls=ctls_$c(d)
	f  d  q:$l(%ZD)
	. r !,"Input device: <terminal>: ",%ZD,!
	. i '$l(%ZD) s %ZD=$p q
	. i %ZD="^" q
	. i %ZD="?" d  q
	 . . w !!,"Select the device you want for input"
	 . . w !,"If you wish to exit enter a caret (^)",!
	 . . s %ZD=""
	. i $zparse(%ZD)="" w "  no such device" s %ZD="" q
	. o %ZD:(readonly:block=2048:record=2044:exception="g noopen"):0
	. i '$t  w !,%ZD," is not available" s %ZD="" q
	. q
noopen	. w !,$p($ZS,",",2,999),! c %ZD s %ZD=""
	q:%ZD="^"
	w !!
	s sav="",(g,n)=0
	u %ZD:exception="g eof"
	r x,y u $p w !,x,!,y,!!
	u $p r !,"OK <Yes>? ",x,!!
	i $l(x),$e("NO",1,$l(x))=$tr(x,"no","NO") c:%ZD'=$p %ZD u $p:(ctrap="":exc="") q
	s fmt=y["ZWR"
	i (fmt)  f  u %ZD  r x  q:x=""  d
	. s @x,n=n+1,x=$p($p(x,"="),"(")  i x'=sav,x'="^" d
	. . s g=g+1,sav=x
	. . u $p w:$x>70 ! w x,?$x\10+1*10
	i ('fmt)  f  u %ZD r x,y i "*"'[$e(x) d
	. i $tr(x,ctls,"")'=x d		;convert control chars to $C(x) exprs
	. . n c,cp,nx s nx=""
	. . f cp=1:1:$l(x) d
	. . . s c=$e(x,cp),nx=nx_$s(ctls[c:"""_$c("_$a(c)_")_""",1:c)
	. . s x=nx	;use fixed 'x'
	. s @x=y
	. s n=n+1,x=$p(x,"(")
	. i x'=sav,x'="^" d
	. . s g=g+1,sav=x
	. . u $p w:$x>70 ! w x,?$x\10+1*10
eof	u $p
	w !!,"Restored ",n," node",$s(n=1:"",1:"s")
	w " in ",g," global",$s(g=1:".",1:"s.")
	c:%ZD'=$p %ZD u $p:(ctrap="":exc="")
	q
	;
ERR	u $p w !,$p($zs,",",2,99),!
	; Warning - Fall-though
	s $ec=""
EXIT	i $d(%ZD),%ZD'=$p c %ZD
	u $p:(ctrap="":exc="")
	q
