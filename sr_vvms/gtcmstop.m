;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;                                                               ;
;       Copyright 1990, 2002 Sanchez Computer Associates, Inc.   ;
;                                                               ;
;       This source code contains the intellectual property     ;
;       of its copyright holder(s), and is made available       ;
;       under a license.  If you do not know the terms of       ;
;       the license, please stop and do not read further.       ;
;                                                               ;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
GTCMSTOP	;GT.CM STOP utility - stops the GT.CM Server
	; If a logical GTCMSVRNAM is defined, only the process pointed by
	; it will be stopped (note: it is not checked whether that is a
	; GTCM_SERVER process). If the logical GTCMSVRNAM is not defined,
	; all processes with image name GTCM_SERVER are stopped.
	; All processes that are being stopped will be listed.
	;
	n i,op,expriv,nopriv,pid,zt,$zt
	s $zt="zg "_$zl_":ERR^GTCMSTOP" u $p:ctrap=$c(3) w !!
	s op=$zsetprv("GROUP,WORLD"),(expriv,nopriv)=0,zt="d blindpid"
	f i=1:1:10 d getipid(.pid) q:'pid  d
	. i i<10 d stopem(.pid) h 2 q
	. d killem(.pid)
	i expriv w !,"Insufficient privileges to examine ",expriv,"process",$s(expriv>1:"es.",1:"."),!
	i nopriv w !,"Insufficient privileges to stop ",nopriv," process",$s(nopriv>1:"es.",1:"."),!
	s op=$zsetprv(op) u $p:ctrap=""
	q
getipid(t);
	n p,$zt,found,iorp k t s $zt=zt,t=0
	s p=$zpid(0),logname=$ztrnlnm("GTCMSVRNAM") q:'p
	f  d  s p=$zpid(1) q:'p  d
	. q:'p  s iname=$zparse($zgetjpi(p,"IMAGNAME"),"NAME"),pname=$zgetjpi(p,"PRCNAM"),found=0,iorp=0
	. i logname'="" s iorp=1
	. i iorp,pname=logname s found=1
	. i 'iorp,iname="GTCM_SERVER" s found=1
	. i found s t(p)=$$FUNC^%DH(p,8),t=t+1
	q
blindpid
	i $zs["NOPRIV" s expriv=expriv+1
	e  w !,$$FUNC^%DH(p,8),"  ",$p($zs,",",2,99),!
	s p=0
	q
stopem(t);
	n p s p=""
	f  s p=$o(t(p)) q:p=""  zsystem "show system /full /id="_t(p) d msg($&FORCEX(p),t(p),"Stopping process ")
	q
killem(t);
	n p s p=""
	f  s p=$o(t(p)) q:p=""  zsystem "show system /full /id="_t(p) d msg($&DELPRC(p),t(p),"Deleting process ")
	q
msg(stat,prc,defmsg)
	i stat=1 w defmsg,prc,! q
	s stat=$zm(stat)
	i stat["NOPRIV" s:'$d(nopriv(prc)) nopriv(prc)=1,nopriv=nopriv+1 q
	w "Error for ",prc," : ",stat,!
	q
ERR	w !,$p($zs,",",2,99),! u $p:ctrap="" s:$d(op) op=$zsetprv(op)
	q
