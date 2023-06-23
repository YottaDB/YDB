;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
; Copyright (c) 1987-2019 Fidelity National Information		;
; Services, Inc. and/or its subsidiaries. All rights reserved.	;
;								;
; Copyright (c) 2020-2022 YottaDB LLC and/or its subsidiaries.	;
; All rights reserved.						;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%RSEL	;GT.M %RSEL utility - routine select into a local array
	;invoke ^%RSEL to create %ZR - a local array of existing routines, interactively
	;invoke OBJ^%RSEL to create %ZR based on object modules
	;invoke CALL^%RSEL to maintain the array %ZR with the input %ZR, %ZE specifies extensions .m or .o[bj]
	;
	; For https://gitlab.com/YottaDB/DB/YDB/-/issues/781 (^%RSEL/^%RD
	; includes routines in shared library files), the routine runs utility
	; file and nm from GNU binutils in PIPE devices to obtain routines in
	; shared libraries and store them in local variable shlstash - see label
	; shlstash(). From shlstash, when searching for object routines per
	; specifications in labels CALL, OBJ, or SILENT routines are copied
	; into nodes of shlib.
	;
	; The changes attempt to fit into the existing code instead of creating
	; a new routine.
	;
	; There is some potentially unintuitive code to:
	; - Mimic the stateful search of $ZSEARCH() which the label next calls
	;   repeatedly. Nodes shlib(<shlibfile>,type) track the state of the scan
	;   through routine names, and nodes shlib(<shlibfile>,type,<routine>)
	;   track routines that are found matching search criteria.
	; - Mimic the pattern matching of ZSEARCH(). The labels mpatt() and
	;   mquote() together create M patterns that duplicate the pattern
	;   matching of $ZSEARCH(), at least as far as matching M routines is
	;   concerned, though not in the general case.
	; - Handle the fact that M routines starting with "%" are stored in
	;   files starting with "_". While "%" precedes "A" in ASCII, "_" is
	;   between "Z" and "a". So, routines starting with "_" are stored in a
	;   different subtrees of shlstash and shlib. Subtrees where the second
	;   subscript (type) is 1 store % routines, whereas those where the
	;   subscript is 2 store other routines. The variables undbeg and
	;   undstop are twins of beg and stop, with any leading "%" replaced
	;   with "_".
	;
	; There is special code for handling routines starting with "__". This
	; is historical code that has no apparent purpose. However, it was left
	; in place as removing it would make harder merging future versions of
	; the upstream code base.
SRC
	n $et s $et=$s($d(%zdebug):"b",1:"zg "_$zl_":err^%RSEL")
	n add,beg,cnt,ctrap,d,delim,end,exc,from,i,k,last,mtch,pct,r,rd,rdf,out,scwc,shlstash,stack,stop,strt,to,%ZE
	s %ZE=".m"
	k %ZR
	i $d(%ZRSET) k ^%RSET($j)
	d init,main
	q
OBJ
	n $et s $et=$s($d(%zdebug):"b",1:"zg "_$zl_":err^%RSEL")
	n add,beg,cnt,ctrap,d,delim,end,exc,from,i,k,last,mtch,pct,r,rd,rdf,out,scwc,shlstash,stack,stop,strt,to,%ZE
	s %ZE=".o"
	k %ZR
	i $d(%ZRSET) k ^%RSET($j)
	d init,main
	q
RD
	n $et s $et=$s($d(%zdebug):"b",1:"zg "_$zl_":err^%RSEL")
	n add,beg,cnt,ctrap,d,delim,end,exc,from,i,k,last,mtch,pct,r,rd,rdf,out,scwc,shlstash,stack,stop,strt,to,%ZRSET
	w !,"Routine directory"
	d init
	s (out,rd,rdf)=1
	i $l($g(%ZR)) w ! d work w !,"Total of ",cnt," routine",$s(cnt=1:".",1:"s."),! q
	d main
	i rdf s %ZR="*" d work W !,"Total of ",cnt," routine",$s(cnt=1:".",1:"s."),!
	q
CALL
	n add,beg,cnt,ctrap,d,delim,end,exc,from,i,k,last,mtch,pct,r,rd,rdf,out,scwc,shlstash,stack,stop,strt,to n:'$d(%ZE) %ZE
	i $g(%ZE)'[".o" s %ZE=".m"
	d init
	i $d(%ZRSET) d  i $l($g(^%RSET($j))) s out=0 d main s ^%RSET($j)=cnt q
	. i $d(^%RSET($j))>1 s r="" f  s r=$o(^%RSET($j,r)) q:'$l(r)  s cnt=cnt+1
	e  d  i $l($g(%ZR)) s out=0 d main s:'$d(%ZRSET) %ZR=cnt s:$d(%ZRSET) ^%RSET($j)=cnt q
	. i $d(%ZR)>1 s r="" f  s r=$o(%ZR(r)) q:'$l(r)  s cnt=cnt+1
	d main
	q
SILENT(patt,label)
	n $et,io s $et=$s($d(%zdebug):"b",1:"zg "_$zl_":err^%RSEL"),io=$io
	i ""=$g(label) s label="SRC"
	d @label
	u io
	q
SRCDIR()
	n i,j,piece,plen,srcdir
	s srcdir=""
	f i=1:1:$zl($zro,"(") s piece=$zpi($zpi($zro,"(",i+1),")",1) d
	. f j=1:1:$zl(piece," ") s srcdir=srcdir_" "_$zpi(piece," ",j)
	q $ze(srcdir,2,$zlength(srcdir))
init
	s delim=" ",scwc="?",from=" !""#$&'()+'-./;<=>@[]\^_`{}|~",to=""
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
	f k=1:1:$l($zro,delim) d  s:$l(r)&'((%ZE[".m")&(r[".so")) d=d+1,d(d)=$p(r,"*")
	. s r=$tr($p($zro,delim,k),"*")
	. i delim=" " d  s:$l(r) r=$zparse(r_$s(r?.E1".so":"",1:"/"),"","*") q	; UNIX conventions
	. . i r'["(" d:(%ZE[".o")&(r?.E1".so") shlstash(r) q			; it has both src and obj, or is a shared lib
	. . i %ZE[".o" d  q							; only want objects
	. . . s r=$p(r,"(")							; grab object directory
	. . . f k=k:1:$l($zro,delim) q:$p($zro,delim,k)[")"			; and step over source info
	. . s r=$p(r,"(",2)							; grab 1st souce directory
	. . i r[")" s r=$p(r,")") q						; it's the only one - we're done
	. . d  f k=k+1:1 s r=$p($zro,delim,k) i $l(r) d  i r[")" s r=$p(r,")") q  ; record all but the last
	. . . i r'[")" s r=$p($zparse(r_"/","","*"),"*") i $l(r) s d=d+1,d(d)=r
	q
main
	set i="zg "_$zl_":main^%RSEL:$zstatus[""CTRAP"","_$zl
	set i=i_":err^%RSEL:'$io=$p s:'$zeof&($zstatus'[""TERMWRITE"") x=$zjobexam(""utilfail"") h"
	u:'$d(%zdebug) $p:(ctrap=$c(3):exception=i)
	s mtch="__" d start(0)
	f  d  q:'$l(%ZR)
	. zsh "s":stack								; get the current stack
	. i $p(stack("S",1),"^",2)=$p($g(stack("S",4)),"^",2) s %ZR=patt,patt="",out=0 k stack q:'$l(%ZR) ; if silent, don't prompt
	. e  r !,"Routine: ",%ZR,! q:'$l(%ZR)
	. i $e(%ZR)="?" d help q
	. d work
	. i '$d(stack) q							; if silent, don't output count
	. e  w !,$s(rd:"T",1:"Current t"),"otal of ",cnt," routine",$s(cnt=1:".",1:"s."),!
	i $D(%ZRSET) s ^%RSET($j)=cnt k %ZR
	e  s %ZR=cnt
	u $p:(ctrap=ctrap:exception=exc)
	q
work
	n filpatt,undbeg,undstop,shlib
	i rd s add=1,cnt=0,r=%ZR k %ZR s %ZR=r ; This behavior is a bit odd
	e  i "'-"[$e(%ZR) s add=0,r=$e(%ZR,2,999)
	e  s add=1,r=%ZR
	s r=$tr(r,from,to)	; strip out invalid characters
	; In addition, filter out "all" non-ascii characters (irrespective of M or UTF-8)
	n r1,c s r1="" f k=1:1:$l(r) s c=$e(r,k) if $a(c)<128 s r1=r1_c
	s r=r1
	s end=$p(r,":",2),beg=$p(r,":"),undbeg=$tr($e(beg),"%","_")_$e(beg,2,$l(beg)),rdf=0
	i end=beg!'$l(end) q:'$l(beg)  s stop=last,undstop=$tr($e(stop),"%","_")_$e(stop,2,$l(stop)) ; if all stripped out, done
	s:'$l(end) filpatt=$$mpatt(undbeg)
	s:'$l(beg) (beg,undbeg)="*" s pct=$e(beg)                                                    ; CAUTION: ELSE on next line
	e  s strt=$$mask(beg),stop=$$mask(end),undstop=$tr($e(stop),"%","_")_$e(stop,2,$l(stop)) i $l($p(stop,"$")) q:stop']strt ; if end before beginning, done
	i "*?"[pct s mtch="%*" d start(1) f  s r=$$search(1) q:r]stop!'$l(r)  d save        ; if alls, get _files first
	s pct=pct="%",mtch=beg
	d start(pct)
	f  s r=$$search(pct) q:r]stop!'$l(r)  d save                                        ; do beginning
	i stop=last,undstop=$tr($e(stop),"%","_")_$e(stop,2,$l(stop)) q                     ; no range - we're done
	s stop=$p(stop,"$"),undstop=$tr($e(stop),"%","_")_$e(stop,2,$l(stop))
	i $l(stop),stop]$p(strt,"$") d                                                      ; if no overlap, do middle
	. s strt=$tr(strt,"$",last)
	. i pct s mtch="%*" d start(1) f  s r=$$search(1) q:stop']r!'$l(r)  i r]strt d save
	. i $e(end)'="%" s mtch="*" d start(0) f  s r=$$search(0) q:stop']r!'$l(r)  i r]strt d save
	e  s strt=$p(strt,"$") i '$l(strt) s strt="$"
	s pct=$e(end)="%",mtch=end
	d start(pct)
	f  s r=$$search(pct) q:'$l(r)  i strt']r d save				; and finish
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
next(k,pct)
	n t,shlibrtn,shlstashrtn,type
	s t=""
	i "/"=$ze(d(k),$zl(d(k))) d
	. f  s t=$zsearch(d(k)_mtch,k) q:t=""  s t=$zparse(t,"NAME") q:t?1A.AN!pct  i scwc="%",$e(t)]"Z" s t="" q
	e  d:".o"=%ZE		       ; Search shared libraries only when object code is sought
	. i "__.o"=mtch s t=$g(shlstash(d(k),0,"__.o"),"")	 ; if looking for __.o, just return if it exists
	. ; Return routine from shlib, copying entries from shlstash to shlib as needed
	. ; See more comments at label shlstash
	. e  d
	. . s type=$s(mtch?1"_".E1".o":1,1:2)
	. . i $data(filpatt) d:'$data(shlib(d(k),type))
	. . . s (shlib(d(k),type),shlstashrtn)=""
	. . . f  s shlstashrtn=$o(shlstash(d(k),type,shlstashrtn)) q:""=shlstashrtn  s:shlstashrtn?@filpatt shlib(d(k),type,shlstashrtn)=""
	. . e  d
	. . . s:'$d(shlib(d(k),type)) shlib(d(k),type)=""
	. . . s shlibrtn=$o(shlib(d(k),type,"")),shlstashrtn=$o(shlstash(d(k),type,$o(shlstash(d(k),type,undbeg),-1)))
	. . . d:(shlstashrtn]shlibrtn)&(undbeg']shlstashrtn)&(shlstashrtn']undstop)
	. . . . ; Note: next line relies on YottaDB short circuit evaluation of '$zl(shlstashrtn) before $d(...)
	. . . . f  d  q:'$zl(shlstashrtn)!$d(shlib(d(k),type,shlstashrtn))!(shlstashrtn]undstop)
	. . . . . s shlib(d(k),type,shlstashrtn)="",shlstashrtn=$o(shlstash(d(k),type,shlstashrtn))
	. . . s shlibrtn=$o(shlib(d(k),type,""),-1),shlstashrtn=$o(shlstash(d(k),type,$o(shlstash(d(k),type,shlibrtn),-1)))
	. . . d:(shlstashrtn]shlibrtn)&(undstop]shlibrtn)
	. . . . s shlstashrtn=$o(shlstash(d(k),type,shlibrtn))
	. . . . f  q:'$zl(shlstashrtn)!(undstop]shlstashrtn)  d
	. . . . . s shlib(d(k),type,shlstashrtn)="",shlstashrtn=$o(shlstash(d(k),type,shlstashrtn))
	. . s (t,shlib(d(k),type))=$o(shlib(d(k),type,shlib(d(k),type)))
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
	if $io=$principal,(($zstatus["IOEOF")!($zstatus["TERMWRITE")) halt
	u $p:(ctrap=ctrap:exception=exc)
	w !,$p($ZS,",",2,999),!
	s $ec=""
	q
	;
mpatt(fpatt)
	; Convert a wildcard of the type supported by %RSEL / $ZSEARCH() into an M pattern
	;   - prefix/suffix 1 followed by quoted prefix/suffix
	;   - ? is 1AN
	;   - * is .AN
	; Except if we just have a single *, then provide a pattern that can return % routines (_ in obj files)
	; Note: consecutive occurrences of 1AN can be combined to improve performance
	if fpatt="*" quit "0.1""_"".AN"
	new i,mpatt,next,q,s,tmp
	set i=1,mpatt=""
	for  set q=$find(fpatt,"?",i),s=$find(fpatt,"*",i) quit:'(q!s)  do
	. if ('s)!(q&(q<s)) do	; next wildcard is "?"
	. . set tmp=$$mquote($extract(fpatt,i,q-2))
	. . set mpatt=mpatt_$select($zlength(tmp):1_tmp,1:"")_"1AN"
	. . set i=q
	. else  do		; next wildcard is "*"
	. . set tmp=$$mquote($extract(fpatt,i,s-2))
	. . set mpatt=mpatt_$select($zlength(tmp):1_tmp,1:"")_".AN"
	. . set i=s
	set:i'>$length(fpatt) mpatt=mpatt_1_$$mquote($extract(fpatt,i,$length(fpatt)))
	quit $select('$length(mpatt):1_$$mquote(fpatt),""""=$extract(mpatt,1):1_mpatt,1:mpatt)

mquote(str)
	quit:'$zlength(str) ""
	new zstr
	set zstr=$zwrite(str)
	quit $select(""""=$extract(zstr,1):zstr,1:""""_zstr_"""")

shlstash(file)
	; If file is a shared library, stash a list in caller's variable shlstash of all M routines.
	; From shlstash, next() copies nodes to shlib as need to satisfy search criteria.
	; Uses caller variables: shlib, shlstash
	n io,line,rtn,type
	s io=$io
	s file=$zparse(file)
	o "pipe":(shell="/bin/sh":command="file -L "_file:readonly)::"pipe"
	u "pipe" r line u io c "pipe"
	d:line["shared object"
	. o "pipe":(shell="/bin/sh":command="nm "_file:readonly)::"pipe" u "pipe"
	. f  r line q:$zeof  d
	. . s rtn=$zpi(line," T ",2)
	. . i $zl(rtn) s type=$s(rtn?.AN:2,rtn?1"_".AN:1,rtn?1"__".AN:0,1:-1),shlstash(file,type,rtn)=""
	. u io c "pipe"
	q
