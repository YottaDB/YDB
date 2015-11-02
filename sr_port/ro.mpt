;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 1987, 2011 Fidelity Information Services, Inc	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%RO     ;routine output
        ;last modified by R. Partridge
        ;invoke ^%RO to get interaction
        ;invoke CALL^%RO with %ZR - routine array,
        ;%ZC - strip comments, %ZD - device, %ZH - header label
        ;
        w !,"Routine Output - Save selected routines into RO file.",!
        n ctrap,exc,d,k
        d dev
        i '$d(%zdebug) n $et s $et="zg "_$zl_":ERR^%RO" u $p:(ctrap=$c(3):exc="zg "_$zl_":EXIT^%RO")
        n ename,fl,l,lc,out,r,rn,rf,src,x,y,t1,%ZC,%ZD,%ZH,%ZR,%ZRSET
        s (fl,lc,r)=0,out=1
        d main
        q
        ;
CALL    n ctrap,exc,d,k
        d dev
        n ename,fl,l,lc,out,r,rn,rf,src,x,y,t1,%ZRSET
        n:'$d(%ZC) %ZC n:'$d(%ZD) %ZD n:'$d(%ZH) %ZH
        s %ZC=$g(%ZC,1),%ZD=$g(%ZD,$p),%ZH=$g(%ZH),(fl,lc,r,out)=0
        o %ZD:(newversion:block=2048:record=2044):0 e  q
        i $d(%ZR)<10 d CALL^%RSEL q:$d(%ZR)<10
        d work
        q
        ;
FL      w !,"First Line Lister",!
        n ctrap,exc,d,k
        d dev
        i '$d(%zdebug) n $et s $et="zg "_$zl_":ERR^%RO" u $p:(ctrap=$c(3):exc="zg "_$zl_":EXIT^%RO")
        n ename,fl,l,lc,out,r,rf,rn,src,x,y,t1,%ZC,%ZD,%ZH,%ZR,%ZRSET
        s (lc,r)=0,(%ZC,fl,out)=1,%ZH="Routine First Line Lister Utility"
        d main
        u $p:(ctrap=ctrap:exc=exc)
        q
        ;
main
        s %ZR="" d CALL^%RSEL
        i %ZR=0 w !,"No routines selected" q
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
noopen  . w !,$p($ZS,",",2,999),! c %ZD s %ZD=""
        q:%ZD="^"
        i 'fl d
        . r !,"Header Label: ",%ZH
        . r !,"Strip comments <No>?: ",%ZC
        . i $l(%ZC),"\YES"[("\"_$tr(%ZC,"yes","YES")) s %ZC=0
        . e  s %ZC=1
        w !
        d work
        q
        ;
work    i '$l(%ZH) s %ZH="%RO Routine Output Utility"
        u %ZD w %ZH,!,"GT.M ",$zd($h,"DD-MON-YEAR 24:60:SS"),! u $p
        s %ZR=""
        f  s %ZR=$o(%ZR(%ZR)) q:'$l(%ZR)  d out
        i 'fl u %ZD w !!
        u $p
        i out d
        . w !!,"Total of ",lc," line",$s(lc=1:"",1:"s")
        . w " in ",r," routine",$s(r=1:".",1:"s."),!!
        c:%ZD'=$p %ZD u $p:(ctrap=ctrap:exc=exc)
        q
        ;
out     s rf=%ZR(%ZR)_$tr($e(%ZR),"%","_")_$e(%ZR,2,9999)_".m"
        o rf:(readonly:rewind:exception="g rnoopen")
        u rf:exception="g reof" r x s rn=$p($p($p(x,$c(9))," "),"(") d frmt
        i $tr(rn,"abcdefghijklmnopqrstuvwxyz","ABCDEFGHIJKLMNOPQRSTUVWXYZ")'=%ZR s rn=%ZR
        i %ZD'=$p u $p w:$x>70 ! w %ZR,?$x\10+1*10
        s r=r+1,lc=lc+1 s:x="" x=rn
        u %ZD w rn,!,x,!
        ; warning - loop terminated by an execption
        i 'fl f  u rf r x d frmt i $l(x) u %ZD w x,! s lc=lc+1
        f  u rf r x d frmt q:x'?.E1";"1.E  u %ZD w x,! s lc=lc+1
        u %ZD w ! c rf
        q
        ;
reof    u %ZD w ! c rf
rnoopen i $zs'["EOF" w !,$p($zs,",",2,999),!
        q
frmt    new stripped set stripped=0
        if '%ZC set t1=0 do                         ;strip comments
        . for  set t1=$find(x,";",t1) quit:'t1  if $length($extract(x,1,t1-1),"""")#2 do  quit
        . . if $extract(x,t1)'=";" set x=$extract(x,1,t1-2) if '$length($translate(x," "_$char(9))) set x="",stripped=1
        if '$length(x),stripped quit
        ;kill stripped
        if '$length(x) set x=" " quit
        if $extract(x)=";" set x=" "_x                   ;if lonely comment, provide ls
        set t1=0
        for  set t1=$find(x,$char(9),t1) quit:'t1  do         ;convert <TAB>s to spaces
        . set x=$extract(x,1,t1-2)_$justify("",8-(t1-2#8))_$extract(x,t1,9999)
        quit
        ;
ERR     u $p w !,$p($zs,",",2,99),!
        s $ec=""
        ; Warning - Fall-though
EXIT    i $d(%ZD),%ZD'=$p c %ZD
        i $d(rf) c rf
        u $p:(ctrap=ctrap:exc=exc)
        q
dev     ; save device parameters EXC, and CTRAP
        zsh "d":d
        s d=""
        f  s d=$o(d("D",d)) q:d=""  i $p=$p(d("D",d)," ") s d=d("D",d),ctrap=$p($p(d,"CTRA=",2)," "),exc=$p(d,"EXCE=",2) q
        e  s (ctrap,exc)="" ; should never happen
        s k=$l(exc,"""")
        s k=$l(exc) i $e(exc,1,1)="""",$e(exc,k,k)="""" s exc=$e(exc,2,k-1)
        if ctrap'="" s exc="s ctrap="_ctrap x exc
        k d
        q
