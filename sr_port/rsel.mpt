;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 1987, 2006 Fidelity Information Services, Inc	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%RSEL   ;service@greystone.com gtm,%rsel;19920523;GT.M %RSEL utility - routine select into a local array
        ;invoke ^%RSEL to create %ZR - a local array of existing routines, interactively
        ;invoke OBJ^%RSEL to create %ZR based on object modules
        ;invoke CALL^%RSEL to maintain the array %ZR with the input %ZR, %ZE specifies extensions .m or .o[bj]
        ;
SRC
        n $et s $et=$s($d(%zdebug):"b",1:"zg "_$zl_":err^%RSEL")
        n add,beg,cnt,ctrap,d,delim,end,exc,from,i,k,last,mtch,pct,r,rd,rdf,out,scwc,stack,stop,strt,to,%ZE
        s %ZE=".m"
	k %ZR
	i $d(%ZRSET) k ^%RSET($j)
        d init,main
        q
OBJ
        n $et s $et=$s($d(%zdebug):"b",1:"zg "_$zl_":err^%RSEL")
        n add,beg,cnt,ctrap,d,delim,end,exc,from,i,k,last,mtch,pct,r,rd,rdf,out,scwc,stack,stop,strt,to,%ZE
        s %ZE=$s($zver["VMS":".obj",1:".o")
	k %ZR
	i $d(%ZRSET) k ^%RSET($j)
        d init,main
        q
RD
        n $et s $et=$s($d(%zdebug):"b",1:"zg "_$zl_":err^%RSEL")
        n add,beg,cnt,ctrap,d,delim,end,exc,from,i,k,last,mtch,pct,r,rd,rdf,out,scwc,stack,stop,strt,to,%ZRSET
	w !,"Routine directory"
        d init
        s (out,rd,rdf)=1
        i $l($g(%ZR)) w ! d work w !,"Total of ",cnt," routine",$s(cnt=1:".",1:"s."),! q
        d main
	i rdf s %ZR="*" d work W !,"Total of ",cnt," routine",$s(cnt=1:".",1:"s."),!
        q
CALL
        n add,beg,cnt,ctrap,d,delim,end,exc,from,i,k,last,mtch,pct,r,rd,rdf,out,scwc,stack,stop,strt,to n:'$d(%ZE) %ZE
        i $g(%ZE)'[".o" s %ZE=".m"
        d init
	i $d(%ZRSET) d  i $l($g(^%RSET($j))) s out=0 d main s ^%RSET($j)=cnt q
        . i $d(^%RSET($j))>1 s r="" f  s r=$o(^%RSET($j,r)) q:'$l(r)  s cnt=cnt+1
	e  d  i $l($g(%ZR)) s out=0 d main s:'$d(%ZRSET) %ZR=cnt s:$d(%ZRSET) ^%RSET($j)=cnt q
        . i $d(%ZR)>1 s r="" f  s r=$o(%ZR(r)) q:'$l(r)  s cnt=cnt+1
        d main
        q
SILENT(patt,label)
        n $et s $et=$s($d(%zdebug):"b",1:"zg "_$zl_":err^%RSEL")
	i ""=$g(label) s label="SRC"
	d @label
	q
init
	i $zver["VMS" d
	. s delim=",",scwc="%",from="abcdefghijklmnopqrstuvwxyz !""#$&'()+'-./;<=>?@[]\^_`{}|~",to="ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	e  d
	. s delim=" ",scwc="?",from=" !""#$&'()+'-./;<=>@[]\^_`{}|~",to=""
	s from=from_$c(0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,127)
	zsh "d":d
	s d=""
	f  s d=$o(d("D",d)) q:d=""  i $p=$p(d("D",d)," ") s d=d("D",d),ctrap=$p($p(d,"CTRA=",2)," "),exc=$p(d,"EXCE=",2) q
	e  s (ctrap,exc)="" ; should never happen
	s k=$l(exc,"""")
	s k=$l(exc) i $e(exc,1,1)="""",$e(exc,k,k)="""" s exc=$e(exc,2,k-1)
	if ctrap'="" s exc="s ctrap="_ctrap x exc
	k d
	s (cnt,rd)=0,out=1,(last,r(0))=$c(255)
	i '$l($zro) s d=1,d(1)="" q
        s d=0
	f k=1:1:$l($zro,delim) d  i $l(r) s d=d+1,d(d)=$p(r,"*")
	. s r=$p($zro,delim,k)
	. i delim=" " d  s:$l(r) r=$zparse(r_"/","","*") q                        ; UNIX conventions
	. . i r'["(" q                                                            ; no source info - it does both
	. . i %ZE[".o" d  q                                                       ; only want objects
	. . . s r=$p(r,"(")                                                       ; grab object directory
	. . . f k=k:1:$l($zro,delim) q:$p($zro,delim,k)[")"                       ; and step over source info
	. . s r=$p(r,"(",2)                                                       ; grab 1st souce directory
	. . i r[")" s r=$p(r,")") q                                               ; it's the only one - we're done
	. . d  f k=k+1:1 s r=$p($zro,delim,k) i $l(r) d  i r[")" s r=$p(r,")") q  ; record all but the last
	. . . i r'[")" s r=$p($zparse(r_"/","","*"),"*") i $l(r) s d=d+1,d(d)=r
	. e  d  s:$l(r) r=$zparse(r,"","*") q                                     ; VMS conventions
	. . i r[".olb" s r="" q                                                   ; it's an object library and we don't poke in them
	. . i r'["/" q                                                            ; no souces info - it does both
	. . i %ZE[".o" d  q                                                       ; only want objects
	. . . s r=$p(r,"/")                                                       ; grab the object directory
	. . . f k=k:1:$l($zro,delim) q:$p($zro,delim,k)[")"                       ; and step over source info
	. . s r=$p(r,"=",2)                                                       ; grab 1st source directory
	. . i $e(r)'="(" q                                                        ; /SRC or /NOSRC - we're done
	. . s r=$p(r,"(",2)                                                       ; strip the opening (
	. . i r[")" s r=$p(r,")") q                                               ; it's in parens but only one
	. . d  f k=k+1:1 s r=$p($zro,delim,k) i $l(r) d  i r[")" s r=$p(r,")") q  ; record all but the last
	. . . i r'[")" s r=$p($zparse(r,"","*"),"*") i $l(r) s d=d+1,d(d)=r
	q
main
        u:'$d(%zdebug) $p:(ctrap=$c(3):exception="zg "_$zl_":main^%RSEL")
	s mtch="__" d start(0)
        f  d  q:'$l(%ZR)
	. zsh "s":stack                                                                                   ; get the current stack
	. i $p(stack("S",1),"^",2)=$p($g(stack("S",4)),"^",2) s %ZR=patt,patt="",out=0 k stack q:'$l(%ZR) ; if silent, don't prompt
        . e  r !,"Routine: ",%ZR,! q:'$l(%ZR)
        . i $e(%ZR)="?" d help q
        . d work
	. i '$d(stack) q                                                                    ; if silent, don't output count
        . e  w !,$s(rd:"T",1:"Current t"),"otal of ",cnt," routine",$s(cnt=1:".",1:"s."),!
	i $D(%ZRSET) s ^%RSET($j)=cnt k %ZR
	e  s %ZR=cnt
        u $p:(ctrap=ctrap:exception=exc)
        q
work
        i rd s add=1,cnt=0,r=%ZR k %ZR s %ZR=r ; This behavior is a bit odd
        e  i "'-"[$e(%ZR) s add=0,r=$e(%ZR,2,999)
        e  s add=1,r=%ZR
        s r=$tr(r,from,to)	; strip out invalid characters, and, in VMS, xlate lower to upper
	; In addition, filter out "all" non-ascii characters (irrespective of M or UTF-8)
	n r1,c s r1="" f k=1:1:$l(r) s c=$e(r,k) if $a(c)<128 s r1=r1_c
	s r=r1
	s end=$p(r,":",2),beg=$p(r,":"),rdf=0
        i end=beg!'$l(end) q:'$l(beg)  s stop=last                                          ; if all stripped out, done
	s:'$l(beg) beg="*" s pct=$e(beg)                                                    ; CAUTION: ELSE on next line
        e  s strt=$$mask(beg),stop=$$mask(end) i $l($p(stop,"$")) q:stop']strt              ; if end before begining, done
        i "*?"[pct s mtch="%*" d start(1) f  s r=$$search(1) q:r]stop!'$l(r)  d save        ; if alls, get _files first
	s pct=pct="%",mtch=beg
        d start(pct)
	f  s r=$$search(pct) q:r]stop!'$l(r)  d save                                        ; do begining
        i stop=last q                                                                       ; no range - we're done
        s stop=$p(stop,"$")
        i $l(stop),stop]$p(strt,"$") d                                                      ; if no overlap, do middle
	. s strt=$tr(strt,"$",last)
	. i pct s mtch="%*" d start(1) f  s r=$$search(1) q:stop']r!'$l(r)  i r]strt d save
	. i $e(end)'="%" s mtch="*" d start(0) f  s r=$$search(0) q:stop']r!'$l(r)  i r]strt d save
	e  s strt=$p(strt,"$") i '$l(strt) s strt="$"
        s pct=$e(end)="%",mtch=end
	d start(pct)
	f  s r=$$search(pct) q:'$l(r)  i strt']r d save                                    ; and finish
        q
mask(val)
	q $tr($e(val),"*?","$$")_$tr($e(val,2,9999),"*?%","$$$")
	;
start(pct)
	s mtch=$s('pct:mtch,1:"_"_$e(mtch,2,9999))_%ZE
        f k=1:1:d s r(k)=$$next(k,pct)
	q
search(pct)
        s r=last
        f k=d:-1:1 i $l(r(k)) s:r(k)=r(k-1) r(k)=$$next(k,pct) i $l(r(k)),r(k)']r s i=k,r=r(k)
        i r'=last s r(i)=$$next(i,pct)
        e  s r=""
        q r
	;
next(k,pct,t)
	f  s t=$zsearch(d(k)_mtch,k) q:t=""  s t=$zparse(t,"NAME") q:t?1A.AN!pct  i scwc="%",$e(t)]"Z" s t="" q
	q $s('pct:t,$e(t)="_":"%"_$e(t,2,9999),1:"")
        ;
save
	i $d(%ZRSET) d
        . i add,'$d(^%RSET($j,r)) s ^%RSET($j,r)=d(i),cnt=cnt+1
        . e  i 'add,$d(^%RSET($j,r)) k ^%RSET($j,r) s cnt=cnt-1
	. i  i out w:$x>70 ! w r,?$x\10+1*10
        e  d
        . i add,'$d(%ZR(r)) s %ZR(r)=d(i),cnt=cnt+1
        . e  i 'add,$d(%ZR(r)) k %ZR(r) s cnt=cnt-1
	. i  i out w:$x>70 ! w r,?$x\10+1*10
        q
help
        i "Dd"[$e(%ZR,2),$l(%ZR)=2 d  q
        . w ! s r=""
        . f  s r=$o(%ZR(r)) q:'$l(r)  w:$x>70 ! w r,?($x\10+1*10)
        w !,"<RET>",?15,"to leave",!,"* ",?15,"for all",!,"rout ",?15,"for 1 routine",!,"rout1:rout2 ",?15,"for a range"
        w !,"* ",?15,"as wildcard permitting any number of characters"
        w !,scwc,?15,"as a single character wildcard in positions other than the first"
        i rd q
        w !,"' ",?15,"as the 1st character to remove routines from the list"
        w !,"?D ",?15,"for the currently selected routines"
        q
err
        u $p:(ctrap=ctrap:exception=exc) w !,$p($ZS,",",2,999),!
	s $ec=""
        q
