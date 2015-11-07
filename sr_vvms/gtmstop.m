;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 1989, 2002 Sanchez Computer Associates, Inc.	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
GTMSTOP	;GT.M STOP utility - stop all GT.M processes
	;
	d ^%ST
	n i,op,nopriv,pid,$zt
	s $zt="zg "_$zl_":ERR^GTMSTOP" u $p:ctrap=$c(3) w !!
	s op=$zsetprv("SYSLCK,GROUP,WORLD"),nopriv=0
	i '$zpriv("SYSLCK") w !,"You need SYSLCK privilege to run this program.",!
	e  f i=1:1:10 d getpid(.pid) q:'pid  d
	. i i<10 d stopem(.pid) h 2 q
	. d killem(.pid)
	i nopriv w !,"Insufficient privileges to stop ",nopriv," process",$s(nopriv>1:"es.",1:"."),!
	s op=$zsetprv(op) u $p:ctrap=""
	q
getpid(t);
	n l,p k t s t=0
	s l=$zlkid(0)
	i l d   f  s l=$zlkid(1) q:'l  d
	. i $extract($zgetlki(l,"RESNAM"),1,6)="GTM$LM" s p=$zgetlki(l,"PID"),t(p)=$$FUNC^%DH(p,8),t=t+1
	s p=$zgetjpi(0,"pid")
	i $d(t(p)) s t=t-1 k t(p)
	q
stopem(t);
	n p s p=""
	f  s p=$o(t(p)) q:p=""  d msg($&FORCEX(p),t(p),"Stopping process ")
	q
killem(t);
	n p s p=""
	f  s p=$o(t(p)) q:p=""  d msg($&DELPRC(p),t(p),"Deleting process ")
	q
msg(stat,prc,defmsg)
	i stat=1 w defmsg,prc,! q
	s stat=$zm(stat)
	i stat["NOPRIV" s:'$d(nopriv(prc)) nopriv(prc)=1,nopriv=nopriv+1 q
	i $l(stat) w "Error for ",prc," : ",stat,!
	q
ERR	w !,$p($zs,",",2,99),! u $p:ctrap="" s:$d(op) op=$ZSETPRV(op)
	q
