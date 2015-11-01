;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 1989, 2003 Sanchez Computer Associates, Inc.	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%GCE	;GT.M %GCE utility - global change every occurrence
	;
	n c,g,gn,m,n,ns,os,s,sc,sn,%ZD,%ZG,x
	i '$d(%zdebug) n $et s $et="zg "_$zl_":ERR^%GCE" u $p:(ctrap=$c(3):exc="zg "_$zl_":LOOP^%GCE")
	w !,"Global Change Every occurrence",!
	d base
	q
base	f  d main q:'%ZG
	i $d(%ZD),%ZD'=$p c %ZD
	u $p:(ctrap="":exc="")
	q
main	k %ZG d CALL^%GSEL i '%ZG q
	s gn=""
	r !,"Old string: ",os
	i os="" w !,"No string to find - no search done.",! s %ZG=0 q
	i os?.E1C.E w !,"The Old string contains control characters"
	r !,"New string: ",ns
	i ns?.E1C.E w !,"The New string contains control characters"
	f  r !,"Show changed nodes <Yes>? ",x,! q:$e(x)'="?"  d help
	i $l(x) s sc=$e("NO",1,$l(x))'=$e($tr(x,"no","NO"),1,2)
	e  s sc=1
	i sc f  d  q:$l(%ZD)
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
	i sc q:%ZD="^"
	e  s %ZD=$p
	i %ZD'=$p d
	. u %ZD w $zd($h,"DD-MON-YEAR 24:60:SS"),!,"Global Change Every occurrence of:",!,">",os,"<",!,"To:",!,">",ns,"<",! u $p
	f  s gn=$o(%ZG(gn)) q:gn=""  d search
	q
search	w:$x>70 ! w gn,?$x\10+1*10
	s g=gn,(m,n)=0 u %ZD
	i ($d(@g)#10=1) s n=1 d:@g[os change
	f  s g=$q(@g) q:g=""  s n=n+1 d:@g[os change
	i m w !!,m," changes made in total ",n," nodes.",!
	e  w !!,"No changes made in total ",n," nodes.",!
	u $p
	q
change	w:sc !,g,!,"Was : ",@g
	s s=@g,sn="",c=$l(s,os)
	f i=1:1:c-1 s sn=sn_$p(s,os,i)_ns
	s @g=sn_$p(s,os,c),m=m+i
	w:sc !,"Now : ",@g,!
	q
help	w !,"Answer No to this prompt if you do not wish a trail of the changes"
	q
	q
ERR	i $d(%ZD),%ZD'=$p c %ZD
	u $p w !,$p($zs,",",2,99),!
	u $p:(ctrap="":exc="")
	s $ec=""
	q
LOOP	i $d(%ZD),%ZD'=$p c %ZD
	d base
	q
