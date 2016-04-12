;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 1987, 2008 Fidelity Information Services, Inc	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%GO	;service@greystone.com %GO;19920722 21:15;global output
	;Extracts global data to standard global output (GO) file
	;Invoke ^%GO to get interaction
	;possible enhancements:
	;selection by key list, range and/or wildcard, rather than global name
	;callable entry point
	;
	w !,"Global Output Utility",!
	i '$d(%zdebug) n $et s $et="zg "_$zl_":ERR^%GO" u $p:(ctrap=$c(3):exc="zg "_$zl_":EXIT^%GO")
	n g,gn,m,n,%ZD,%ZG,%ZH,fmt,fmtdone
	d ^%GSEL
	i %ZG=0 w !,"No globals selected" q
	r !,"Header Label: ",%ZH,!
	r !,"Output Format: GO or ZWR: ",fmt,!
	i (fmt="")!($e("ZWR",1,$l(fmt))=$tr(fmt,"zwr","ZWR"))  s fmt=1
	e  s fmt=0
	f  d  q:$l(%ZD)
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
	q:%ZD="^"
	w !!
	i '$l(%ZH) s %ZH="%GO Global Output Utility"
	u %ZD w %ZH w:"M"'=$ZCHSET " ",$ZCHSET w !,"GT.M ",$zd($h,"DD-MON-YEAR 24:60:SS")  w:fmt " ZWR" w !
	s gn="",(m,n)=0
	f  s gn=$o(%ZG(gn)) q:gn=""  s g=gn d
	. s fmtdone=0
	. u $p w:$x>70 ! w gn,?$x\10+1*10 u %ZD i $p=%ZD w !
	. s m=m+1
	. i $d(@g)'[0 d   s n=n+1
	 . . i fmt  zwr @g s fmtdone=1
	 . . e  w g,!,@g,!
	. f  s g=$q(@g) q:g=""  d
 	 . . i fmt zwr:'fmtdone @g	; don't zwr if already done for unsubscripted global. It takes care for subscripts too
	 . . e  w g,!,@g,!
	 . . s n=n+1
	u %ZD w !!
	u $p
	w !!,"Total of ",n," node",$s(n=1:"",1:"s")
	w " in ",m," global",$s(m=1:".",1:"s."),!!
	c:%ZD'=$p %ZD u $p:(ctrap="":exc="")
	q
fw(s)	; variables used in this function are: fwlen, s, cc, fastate, isctl, i, thistime
	; initialize this procedure
	s fwlen=$l(s)
	i fwlen=0  w !  q
	i s=+s  w s,!  q
	s cc=$e(s)
	i cc?1C  w "$C(",$a(cc)  s fastate=2
	e  w """",cc  w:cc="""" cc  s fastate=1
	; start the loop to deal with the whole string.
	f i=2:1:fwlen  s cc=$e(s,i,i),isctl=cc?1C  d
	. s thistime=1
	. if fastate=1  d
	 . . if (isctl)  w """_$C(",$a(cc)  s fastate=2,thistime=0
	 . . else  w cc  w:cc="""" cc
	. if (fastate=2)&thistime  d
	 . . if (isctl)!(cc="""")  w ",",$a(cc)
	 . . else  w ")_""",cc  s fastate=1
	if fastate=1  w """",!
	else  w ")",!
	q
	;
ERR	u $p w !,$p($zs,",",2,99),!
	; Warning - Fall-though
	s $ec=""
EXIT	i $d(%ZD),%ZD'=$p c %ZD
	u $p:(ctrap="":exc="")
	q
