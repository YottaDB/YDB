;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
; Copyright (c) 2001-2017 Fidelity National Information		;
; Services, Inc. and/or its subsidiaries. All rights reserved.	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
show:	;implement the verb: SHOW
ALL
	d TEMPLATE,ALLNAME,ALLGBLNAME,ALLREGIO,ALLSEGME,MAP,INSTANCE
	q
COMMANDS
	n tmpreg2,tmpseg2
	m tmpreg2=tmpreg
	m tmpseg2=tmpseg
	n tmpreg,tmpseg
	m tmpreg=tmpreg2
	m tmpseg=tmpseg2
	k tmpreg2,tmpseg2
	s BOL="!"
	set delim="-"
	; show NAMES after GBLNAME to avoid potential NAMRANGEORDER errors in case of non-zero collation
	i '$l($get(cfile)) d
	. f i="@useio",$s(log:"@uself",1:"") q:'$l(i)  u @i d templatec,regionc,segmentc,gblnamec,namec,instc
	e  o cfile:(newversion:exc="w !,$ztatus c cfile zgoto $zl:cfilefail") u cfile d
	. d templatec,regionc,segmentc,gblnamec,namec,instc
	. c cfile
cfilefail:
	s BOL=""
	q
NAME
	n namsdisp,namedispmaxlen
	i '$d(nams(NAME)) zm gdeerr("OBJNOTFND"):"Name":$$namedisp(NAME,0) q
	d n2calc,n2
	i log s BOL="!" u @uself w BOL d n2 w ! u @useio s BOL=""
	q
n2calc:	s namedispmaxlen=0
	d namedisplaycalc(NAME) ; sets namedispmaxlen
	q
n2:	d namehd,namedisplay(NAME)
	q
ALLNAME
	n namedisp,namedispmaxlen
	d SHOWNAM^GDEMAP,n1calc
	d n1
	i log s BOL="!" u @uself w BOL d n1 w ! u @useio s BOL=""
	q
n1calc:	s namedispmaxlen=0
	s s="#"
	f  s s=$o(nams(s)) q:'$zl(s)  d namedisplaycalc(s)
	q
n1:	d namehd
	d namscollatecalc ; sets "namscollate" array
	s s="#" ; skip processing "#" name
	f  s s=$o(namscollate(s)) q:'$zl(s)  d namedisplay(namscollate(s))
	q
namedisplay:(name)
	w !,BOL,?x(1),namsdisp(name),?x(2),nams(name)
	q
namec:
	n s,key,namedisp,namedispmaxlen,namscollate
	d SHOWNAM^GDEMAP
	s s="#"
	w !,"LOCKS "_delim_"REGION=",nams(s)
	d namscollatecalc	; sets "namscollate" array
	d n1calc		; sets "namedispmaxlen" and "namsdisp" array
	s key="#" ; skip processing "#" name
	f  s key=$o(namscollate(key)) q:'$zl(key)  d
	. s s=namscollate(key)
	. i "*"'=s w !,"ADD "_delim_"NAME ",namsdisp(s)," "_delim_"REGION=",nams(s) q
	. i defreg'=nams(s) w !,"CHANGE "_delim_"NAME "_namsdisp(s)_" "_delim_"REGION=",nams(s)
	w !,BOL
	q
GBLNAME
	i '$d(gnams(GBLNAME)) zm gdeerr("OBJNOTFND"):"Global Name":GBLNAME q
	d gn2
	i log s BOL="!" u @uself w BOL d gn2 w ! u @useio s BOL=""
	q
gn2:	d gblnamehd,gblnameeach(GBLNAME)
	q
gblnameeach(gname)	;
	w !,BOL,?x(1),gname,?x(2),$j(gnams(gname,"COLLATION"),4),?x(3),$j($view("YCOLLATE",gnams(gname,"COLLATION")),3)
	q
ALLGBLNA
ALLGBLNAME
	i gnams=0 q
	d gn1
	i log s BOL="!" u @uself w BOL d gn1 w ! u @useio s BOL=""
	q
gn1:	d gblnamehd
	s s="" f  s s=$o(gnams(s)) q:'$l(s)  d gblnameeach(s)
	q
gblnamec:
	i gnams=0 q
	s s=""
	f  s s=$o(gnams(s)) q:s=""  w !,"ADD "_delim_"GBLNAME ",s," "_delim_"COLLATION=",gnams(s,"COLLATION")
	w !,BOL
	q
ALLINSTA
INSTANCE
	i ($d(inst)<10)!(inst=0) q
	d in2
	i log s BOL="!" u @uself w BOL d in2 w ! u @useio s BOL=""
	q
in2:	d insthd,insteach
	q
insteach;
	w !,BOL,?x(1),$$namedisp(inst("FILE_NAME"),0)
	q
instc:
	i ($d(inst)<10)!(inst=0) q
	s s=""
	w !,"CHANGE "_delim_"INSTANCE "_delim_"FILE_NAME=",$$namedisp(inst("FILE_NAME"),1)
	w !,BOL
	q
REGION
	i '$d(regs(REGION)) zm gdeerr("OBJNOTFND"):"Region":REGION q
	d r2
	i log s BOL="!" u @uself w BOL d r2 w ! u @useio s BOL=""
	q
r2:	d regionhd s s=REGION d onereg
	i regs(s,"JOURNAL") d jnlhd d onejnl
	q
ALLREGIO
ALLREGION
	d r1
	i log s BOL="!" u @uself w BOL d r1 w ! u @useio s BOL=""
	q
r1:	d regionhd s jnl=0,s=""
	f  s s=$o(regs(s)) q:'$l(s)  d onereg i regs(s,"JOURNAL") s jnl=1
	i jnl d jnlhd s s="" f  s s=$o(regs(s)) q:'$l(s)  i regs(s,"JOURNAL") d onejnl
	q
onereg:
	w !,BOL,?x(1),s
	w ?x(2),regs(s,"DYNAMIC_SEGMENT")
	w ?x(3),$j(regs(s,"COLLATION_DEFAULT"),4)
	w ?x(4),$j(regs(s,"RECORD_SIZE"),7)
	w ?x(5),$j(regs(s,"KEY_SIZE"),5)
	w ?x(6),$s(regs(s,"NULL_SUBSCRIPTS")=1:"ALWAYS",regs(s,"NULL_SUBSCRIPTS")=2:"EXISTING",1:"NEVER")
	w ?x(7),$s(regs(s,"STDNULLCOLL"):"Y",1:"N")
	w ?x(8),$s(regs(s,"JOURNAL"):"Y",1:"N")
	w ?x(9),$s(regs(s,"INST_FREEZE_ON_ERROR"):"Y",1:"N")
	w ?x(10),$s(regs(s,"QDBRUNDOWN"):"Y",1:"N")
	w ?x(11),$s(regs(s,"EPOCHTAPER"):"Y",1:"N")
	w ?x(12),$s(regs(s,"AUTODB"):"Y",1:"N")
	w ?x(13),$s(regs(s,"STATS"):"Y",1:"N")
	w ?x(14),$s(regs(s,"LOCK_CRIT"):"Sep",1:"DB")
	q
onejnl:
	w !,BOL,?x(1),s,?x(2),$s($zl(regs(s,"FILE_NAME")):$$namedisp(regs(s,"FILE_NAME"),1),1:"<based on DB file-spec>")
	i $x'<(x(3)-1) w !,BOL
	w ?x(3),$s(regs(s,"BEFORE_IMAGE"):"Y",1:"N"),?x(4),$j(regs(s,"BUFFER_SIZE"),5)
	w ?x(5),$j(regs(s,"ALLOCATION"),10)
	w ?x(6),$j(regs(s,"EXTENSION"),10),?x(7),$j(regs(s,"AUTOSWITCHLIMIT"),13)
	w !,BOL
	q
regionc:
	n s,q,val,synval,tmpval,type
	w !,"DELETE "_delim_"REGION "_defreg
	; delete DEFAULT SEGMENT at same time DEFAULT REGION is deleted to avoid potential KEYSIZIS issues when
	; playing the GDE SHOW -COMMANDS output in a fresh GDE session (GTM-7954).
	w !,"DELETE "_delim_"SEGMENT "_defseg
	s s=""
	f  s s=$o(regs(s)) q:'$l(s)  d
	. w !,"ADD "_delim_"REGION ",s
	. s q=""
	. f  s q=$o(syntab("TEMPLATE","REGION",q)) q:""=q  d
	. . s synval=syntab("TEMPLATE","REGION",q)
	. . s val=regs(s,q)
	. . s tmpval=$get(tmpreg(q))
	. . i synval["LIST" d  q
	. . . ; special processing since this option can in turn be an option-list
	. . . n l,list,lval,lsynval,ltype
	. . . i (synval["NEGATABLE")&(0=val) d  q
	. . . . i tmpval=val q
	. . . . w " "_delim_"NO"_q
	. . . s list="",l=""
	. . . f  s l=$o(syntab("TEMPLATE","REGION",q,l)) q:""=l  d
	. . . . s lval=regs(s,l)
	. . . . i $get(tmpreg(l))=lval q
	. . . . s ltype=$get(syntab("TEMPLATE","REGION",q,l,"TYPE"))	; note: possible this is "" for e.g. "BEFORE_IMAGE")
	. . . . i (ltype'="")&($data(typevalue("NUM2STR",ltype))) s list=list_","_l_"="_typevalue("NUM2STR",ltype,lval) q
	. . . . s lsynval=syntab("TEMPLATE","REGION",q,l)
	. . . . i lsynval["NEGATABLE" s list=list_","_$s(lval:"",1:"NO")_l q
	. . . . d listadd(.list,l,lval)
	. . . i list="" q
	. . . w " "_delim_q_"=("_$ze(list,2,MAXSTRLEN)_")"	; strip out leading "," from list hence the 2 in $ze
	. . i tmpval=val q
	. . s type=$get(syntab("TEMPLATE","REGION",q,"TYPE"))	; note: possible this is ""
	. . i (type'="")&($data(typevalue("NUM2STR",type))) w " "_delim_q_"="_typevalue("NUM2STR",type,val) q
	. . i synval["NEGATABLE" w " "_delim_$s(val:"",1:"NO")_q q
	. . d qualadd(" ",delim,q,val)
	w !,BOL
	q
listadd:(list,l,lval)
	i l="FILE_NAME" d
	. ; FILE_NAME has to be inside double-quotes or else GDE will parse the remainder of the line as the file name
	. s list=list_","_l_"="""_$$namedisp(lval,0)_""""	; since file name can have control characters use "namedisp"
	e  s list=list_","_l_"="_lval
	q
qualadd:(prefix,delim,qual,val)
	i qual="FILE_NAME" d
	. ; FILE_NAME has to be inside double-quotes or else GDE will parse the remainder of the line as the file name
	. w " "_delim_q_"="""_$$namedisp(val,0)_""""		; since file name can have control characters use "namedisp"
	e  w prefix_delim_q_"="_val
	q
SEGMENT
	i '$d(segs(SEGMENT)) zm gdeerr("OBJNOTFND"):"Segment":SEGMENT q
	d s2
	i log s BOL="!" u @uself w BOL d s2 w ! u @useio s BOL=""
	q
s2:	d seghd s s=SEGMENT s am=segs(s,"ACCESS_METHOD") d oneseg
	q
ALLSEGME
	d s1
	i log s BOL="!" u @uself w BOL d s1 w ! u @useio s BOL=""
	q
s1:	d seghd s s=""
	f  s s=$o(segs(s)) q:'$l(s)  s am=segs(s,"ACCESS_METHOD") d oneseg
	q
oneseg:
	w !,BOL,?x(1),s,?x(2),$$namedisp(segs(s,"FILE_NAME"),1)
	i $x'<(x(3)-1) w !,BOL
	w ?x(3),segs(s,"ACCESS_METHOD")
	i am="USER" q
	w ?x(4),$s(segs(s,"FILE_TYPE")="DYNAMIC":"DYN",1:"STA")
	w ?x(5),$j(segs(s,"BLOCK_SIZE"),5),?x(6),$j(segs(s,"ALLOCATION"),10),?x(7),$j(segs(s,"EXTENSION_COUNT"),5)
	d @am
	q
BG	w ?x(8),"GLOB=",$j(segs(s,"GLOBAL_BUFFER_COUNT"),4)
	w !,BOL,?x(8),"LOCK=",$j(segs(s,"LOCK_SPACE"),4)
	w !,BOL,?x(8),"RES =",$j(segs(s,"RESERVED_BYTES"),4)
	; For non-encryption platforms, always show FLAG as OFF.
	w !,BOL,?x(8),"ENCR=",$s((encsupportedplat=TRUE&segs(s,"ENCRYPTION_FLAG")):"  ON",1:" OFF")
	w !,BOL,?x(8),"MSLT=",$j(segs(s,"MUTEX_SLOTS"),4)
	w !,BOL,?x(8),"DALL=",$s(segs(s,"DEFER_ALLOCATE"):" YES",1:"  NO")
	w !,BOL,?x(8),"AIO =",$s(segs(s,"ASYNCIO"):"  ON",1:" OFF")
	q
MM	w ?x(8),$s(segs(s,"DEFER"):"DEFER",1:"NODEFER")
	w !,BOL,?x(8),"LOCK=",$j(segs(s,"LOCK_SPACE"),4)
	w !,BOL,?x(8),"RES =",$j(segs(s,"RESERVED_BYTES"),4)
	w !,BOL,?x(8),"ENCR= OFF"
	w !,BOL,?x(8),"MSLT=",$j(segs(s,"MUTEX_SLOTS"),4)
	w !,BOL,?x(8),"DALL=",$s(segs(s,"DEFER_ALLOCATE"):" YES",1:"  NO")
	q
segmentc:
	n s,q,val,synval,tmpval,type,am
	s s=""
	f  s s=$o(segs(s)) q:'$l(s)  d
	. s am=segs(s,"ACCESS_METHOD")
	. w !,"ADD "_delim_"SEGMENT ",s
	. i tmpacc'=am w " "_delim_"ACCESS_METHOD=",am
	. s q=""
	. f  s q=$o(syntab("TEMPLATE","SEGMENT",q)) q:""=q  d
	. . i q="ACCESS_METHOD" q  ; already processed
	. . s synval=syntab("TEMPLATE","SEGMENT",q)
	. . i synval["LIST" d ABORT^GDE	; segmentc is not designed to handle LIST in segment qualifiers.
	. . s val=segs(s,q)
	. . s tmpval=$get(tmpseg(am,q))
	. . i tmpval=val q
	. . s type=$get(syntab("TEMPLATE","SEGMENT",q,"TYPE"))	; note: possible this is ""
	. . i (type'="")&($data(typevalue("NUM2STR",type))) w " "_delim_q_"="_typevalue("NUM2STR",type,val) q
	. . i synval["NEGATABLE" w " "_delim_$s(val:"",1:"NO")_q q
	. . d qualadd(" ",delim,q,val)
	w !,BOL
	q
MAP
	n map,mapdisp,mapdispmaxlen
	i '$d(mapreg) n mapreg s mapreg=""
	e  i '$d(regs(mapreg)) zm gdeerr("OBJNOTFND"):"Region":mapreg q
	d NAM2MAP^GDEMAP,m1
	i log s BOL="!" u @uself w BOL d m1 w ! u @useio s BOL=""
	q
m1:	n l1,s1,s2
	s mapdispmaxlen=0 d mapdispcalc
	d maphd
	s s1=$o(map("$"))
	i s1'="%" s map("%")=map("$"),s1="%"
	f  s s2=s1,s1=$o(map(s2)) q:'$zl(s1)  d onemap(s1,s2)
	d onemap("...",s2)
	i $d(nams("#")) s s2="LOCAL LOCKS",map(s2)=nams("#") d onemap("",s2) k map(s2)
	q
onemap:(s1,s2)
	i $l(mapreg),mapreg'=map(s2) q
	s l1=$zl(s1)
	i $zl(s2)=l1,$ze(s1,l1)=0,$ze(s2,l1)=")",$ze(s1,1,l1-1)=$ze(s2,1,l1-1) q
	i '$d(mapdisp(s1)) s mapdisp(s1)=s1 ; e.g. "..." or "LOCAL LOCKS"
	i '$d(mapdisp(s2)) s mapdisp(s2)=s2 ; e.g. "..." or "LOCAL LOCKS"
	w !,BOL,?x(1),mapdisp(s2),?x(2),mapdisp(s1),?x(3),"REG = ",map(s2)
	i '$d(regs(map(s2),"DYNAMIC_SEGMENT")) d  q
	. w !,BOL,?x(3),"SEG = NONE",!,BOL,?x(3),"FILE = NONE"
	s j=regs(map(s2),"DYNAMIC_SEGMENT") w !,BOL,?x(3),"SEG = ",j
	i '$d(segs(j,"ACCESS_METHOD")) w !,BOL,?x(3),"FILE = NONE"
	e  s s=segs(j,"FILE_NAME") w !,BOL,?x(3),"FILE = ",$$namedisp(s,1)
	q
TEMPLATE
	d t1
	i log s BOL="!" u @uself w BOL d t1 w ! u @useio s BOL=""
	q
t1:	d tmpreghd
	w !,BOL,?x(1),"<default>"
	w ?x(3),$j(tmpreg("COLLATION_DEFAULT"),4)
	w ?x(4),$j(tmpreg("RECORD_SIZE"),7)
	w ?x(5),$j(tmpreg("KEY_SIZE"),5)
	w ?x(6),$s(tmpreg("NULL_SUBSCRIPTS")=1:"ALWAYS",tmpreg("NULL_SUBSCRIPTS")=2:"EXISTING",1:"NEVER")
	w ?x(7),$s(tmpreg("STDNULLCOLL"):"Y",1:"N")
	w ?x(8),$s(tmpreg("JOURNAL"):"Y",1:"N")
	w ?x(9),$s(tmpreg("INST_FREEZE_ON_ERROR"):"Y",1:"N")
	w ?x(10),$s(tmpreg("QDBRUNDOWN"):"Y",1:"N")
	w ?x(11),$s(tmpreg("EPOCHTAPER"):"Y",1:"N")
	w ?x(12),$s(tmpreg("AUTODB"):"Y",1:"N")
	w ?x(13),$s(tmpreg("STATS"):"Y",1:"N")
	w ?x(14),$s(tmpreg("LOCK_CRIT"):"Sep",1:"DB")
	i tmpreg("JOURNAL") d tmpjnlhd,tmpjnlbd
	d tmpseghd
	w !,BOL,?x(1),"<default>"
	w ?x(2),$s(tmpacc="BG":"  *",1:"")
	w ?x(3),"BG"
	w ?x(4),$s(tmpseg("BG","FILE_TYPE")="DYNAMIC":"DYN",1:"STA")
	w ?x(5),$j(tmpseg("BG","BLOCK_SIZE"),5)
	w ?x(6),$j(tmpseg("BG","ALLOCATION"),10)
	w ?x(7),$j(tmpseg("BG","EXTENSION_COUNT"),5)
	w ?x(8),"GLOB =",$j(tmpseg("BG","GLOBAL_BUFFER_COUNT"),4)
	w !,BOL,?x(8),"LOCK =",$j(tmpseg("BG","LOCK_SPACE"),4)
	w !,BOL,?x(8),"RES  =",$j(tmpseg("BG","RESERVED_BYTES"),4)
	w !,BOL,?x(8),"ENCR =",$s((encsupportedplat=TRUE&tmpseg("BG","ENCRYPTION_FLAG")):"  ON",1:" OFF")
	w !,BOL,?x(8),"MSLT =",$j(tmpseg("BG","MUTEX_SLOTS"),4)
	w !,BOL,?x(8),"DALL =",$s(tmpseg("BG","DEFER_ALLOCATE"):" YES",1:"  NO")
	w !,BOL,?x(8),"AIO  =",$s(tmpseg("BG","ASYNCIO"):"  ON",1:" OFF")
	w !,BOL,?x(1),"<default>"
	w ?x(2),$s(tmpacc="MM":"   *",1:"")
	w ?x(3),"MM"
	w ?x(4),$s(tmpseg("MM","FILE_TYPE")="DYNAMIC":"DYN",1:"STA")
	w ?x(5),$j(tmpseg("MM","BLOCK_SIZE"),5)
	w ?x(6),$j(tmpseg("MM","ALLOCATION"),10)
	w ?x(7),$j(tmpseg("MM","EXTENSION_COUNT"),5)
	w ?x(8),$s(tmpseg("MM","DEFER"):"DEFER",1:"NODEFER")
	w !,BOL,?x(8),"LOCK =",$j(tmpseg("MM","LOCK_SPACE"),4)
	w !,BOL,?x(8),"MSLT =",$j(tmpseg("MM","MUTEX_SLOTS"),4)
	w !,BOL,?x(8),"DALL =",$s(tmpseg("MM","DEFER_ALLOCATE"):" YES",1:"  NO")
	q
tmpjnlbd:
	w !,BOL,?x(1),"<default>",?x(2),$s($zl(tmpreg("FILE_NAME")):$$namedisp(tmpreg("FILE_NAME"),1),1:"<based on DB file-spec>")
	i $x'<(x(3)-1) w !,BOL
	w ?x(3),$s(tmpreg("BEFORE_IMAGE"):"Y",1:"N"),?x(4),$j(tmpreg("BUFFER_SIZE"),5)
	w ?x(5),$j(tmpreg("ALLOCATION"),10)
	w ?x(6),$j(tmpreg("EXTENSION"),10),?x(7),$j(tmpreg("AUTOSWITCHLIMIT"),13)
	w !,BOL
	q
templatec:
	n q,synval,tmpval,type,cmd,defercnt,defercmd,i,am,s,freq,freqx,val
	; compute template values that are most common across regions and store these into tmpreg
	s s=""
	f  s s=$o(regs(s)) q:'$l(s)  d
	. s q=""
	. f  s q=$o(tmpreg(q)) q:'$l(q)  d
	. . i tmpreg(q)="" q  ; if this qualifier has a "" template value, then skip processing it (e.g. "FILE_NAME")
	. . s val=regs(s,q)
	. . s freq=$incr(freq(q,val))
	. . s freqx(q,freq)=val
	f  s q=$o(tmpreg(q)) q:'$l(q)  d
	. i tmpreg(q)="" q  ; if this qualifier has a "" template value, then skip processing it (e.g. "FILE_NAME")
	. s freq=$o(freqx(q,""),-1)
	. s val=freqx(q,freq)
	. s tmpreg(q)=val
	; compute template values that are most common across segments and store these into tmpseg
	k freq,freqx
	s s=""
	f  s s=$o(segs(s)) q:'$l(s)  d
	. s am=segs(s,"ACCESS_METHOD")
	. s q=""
	. f  s q=$o(tmpseg(am,q)) q:'$l(q)  d
	. . s val=segs(s,q)
	. . s freq=$incr(freq(am,q,val))
	. . s freqx(am,q,freq)=val
	s am=""
	f  s am=$o(freqx(am)) q:'$l(am)  d
	. s q=""
	. f  s q=$o(tmpseg(am,q)) q:'$l(q)  d
	. . s freq=$o(freqx(am,q,""),-1)
	. . s val=freqx(am,q,freq)
	. . s tmpseg(am,q)=val
	; use more optimal templates (tmpreg/tmpseg) while generating template commands below
	; ---------------------------------------------------------
	; dump TEMPLATE -REGION section
	; ---------------------------------------------------------
	s q=""
	s cmd="TEMPLATE "_delim_"REGION ",defercmd="",defercnt=0
	f  s q=$o(syntab("TEMPLATE","REGION",q)) q:""=q  d
	. s synval=syntab("TEMPLATE","REGION",q)
	. s tmpval=$get(tmpreg(q))
	. i (tmpval="") q	 ; if this qualifier does not have a non-default template value, never mind
	. i synval["LIST" d  q
	. . ; special processing since this option can in turn be an option-list
	. . n l,list,lsynval,ltype
	. . i (synval["NEGATABLE")&(0=tmpval) s defercmd($incr(defercnt))=cmd_delim_"NO"_q
	. . s list="",l=""
	. . f  s l=$o(syntab("TEMPLATE","REGION",q,l)) q:""=l  d
	. . . s lval=tmpreg(l)
	. . . s ltype=$get(syntab("TEMPLATE","REGION",q,l,"TYPE"))	; note: possible this is "" for e.g. "BEFORE_IMAGE")
	. . . i (ltype'="")&($data(typevalue("NUM2STR",ltype))) s list=list_","_l_"="_typevalue("NUM2STR",ltype,lval) q
	. . . s lsynval=syntab("TEMPLATE","REGION",q,l)
	. . . i lsynval["NEGATABLE" s list=list_","_$s(lval:"",1:"NO")_l q
	. . . i (lval="") q	 ; if this qualifier is not applicable to this platform, never mind
	. . . s list=list_","_l_"="_lval
	. . i list="" q
	. . w !,cmd_delim_q_"=("_$ze(list,2,MAXSTRLEN)_")"	; strip out leading "," from list hence the 2 in $ze
	. s type=$get(syntab("TEMPLATE","REGION",q,"TYPE"))	; note: possible this is ""
	. i (type'="")&($data(typevalue("NUM2STR",type))) w !,cmd_delim_q_"="_typevalue("NUM2STR",type,tmpval) q
	. i synval["NEGATABLE" w !,cmd_delim_$s(tmpval:"",1:"NO")_q q
	. w !
	. d qualadd(cmd,delim,q,tmpval)
	w !,BOL
	f i=1:1:defercnt write !,defercmd(i)	; finish off deferred work (if any)
	w !,BOL
	; ---------------------------------------------------------
	; dump TEMPLATE -SEGMENT -ACCESS_METHOD=MM,BG,USER section
	; ---------------------------------------------------------
	s cmd="TEMPLATE "_delim_"SEGMENT "
	s am=""
	f  s am=$o(tmpseg(am)) q:""=am  d
	. w !,cmd,delim_"ACCESS_METHOD=",am
	. s q=""
	. f  s q=$o(syntab("TEMPLATE","SEGMENT",q)) q:""=q  d
	. . i q="ACCESS_METHOD" q  ; already processed
	. . s synval=syntab("TEMPLATE","SEGMENT",q)
	. . i synval["LIST" d ABORT^GDE	; segmentc is not designed to handle LIST in segment qualifiers.
	. . s tmpval=$get(tmpseg(am,q))
	. . i (tmpval="") q	 ; if this qualifier does not have a non-default template value, never mind
	. . s type=$get(syntab("TEMPLATE","SEGMENT",q,"TYPE"))	; note: possible this is ""
	. . i (type'="")&($data(typevalue("NUM2STR",type))) w !,cmd_delim_l_"="_typevalue("NUM2STR",type,tmpval) q
	. . i synval["NEGATABLE" w !,cmd_delim_$s(tmpval:"",1:"NO")_q q
	. . w !
	. . d qualadd(cmd,delim,q,tmpval)
	. w !,BOL
	w !,cmd,delim_"ACCESS_METHOD="_tmpacc
	w !,BOL
	q

;-----------------------------------------------------------------------------------------------------------------------------------

namehd:
	s x(0)=9,x(1)=1,x(2)=$s(x(1)+2+namedispmaxlen'>36:36,1:namedispmaxlen+x(1)+2)
	w !,BOL,!,BOL,?x(0),"*** NAMES ***",!,BOL,?x(1),"Global",?x(2),"Region"
	w !,BOL,?x(1),$tr($j("",$s(x(2)=36:78,1:x(2)+32))," ","-")
	q
gblnamehd:
	s x(0)=9,x(1)=1,x(2)=36,x(3)=42
	w !,BOL,!,BOL,?x(0),"*** GBLNAMES ***",!,BOL,?x(1),"Global",?x(2),"Coll",?x(3),"Ver"
	w !,BOL,?x(1),$tr($j("",78)," ","-")
	q
insthd:
	s x(0)=43,x(1)=1
	w !,BOL,!,BOL,?x(0),"*** INSTANCE ***"
	w !,BOL," Instance File (def ext: .repl)"
	w !,BOL,?x(1),$tr($j("",57)," ","-")
	q
regionhd:
	s x(0)=32,x(1)=1,x(2)=33,x(3)=65,x(4)=71
	s x(5)=79,x(6)=85,x(7)=95,x(8)=100,x(9)=104,x(10)=111,x(11)=117,x(12)=123,x(13)=130,x(14)=136,x(15)=141
	w !,BOL,!,BOL,?x(0),"*** REGIONS ***"
	w !,BOL,?x(7),"Std"
	w ?x(9),"Inst"
	w !,BOL,?x(2),"Dynamic",?x(3)," Def"
	w ?x(4),"    Rec"
	w ?x(5),"  Key"
	w ?x(6),"Null"
	w ?x(7),"Null"
	w ?x(9),"Freeze"
	w ?x(10),"Qdb"
	w ?x(11),"Epoch"
	w ?x(14),"LOCK"
	w !,BOL
	w ?x(1),"Region"
	w ?x(2),"Segment"
	w ?x(3),"Coll"
	w ?x(4),"   Size"
	w ?x(5)," Size"
	w ?x(6),"Subs"
	w ?x(7),"Coll"
	w ?x(8),"Jnl"
	w ?x(9),"on Err"
	w ?x(10),"Rndwn"
	w ?x(11),"Taper"
	w ?x(12),"AutoDB"
	w ?x(13),"Stats"
	w ?x(14),"Crit"
	w !,BOL,?x(1),$tr($j("",139)," ","-")
	q
jnlhd:
	s x(0)=26,x(1)=1,x(2)=33,x(3)=59,x(4)=65,x(5)=71,x(6)=82,x(7)=91
	w !,BOL,!,BOL,?x(0),"*** JOURNALING INFORMATION ***"
	w !,BOL,?x(1),"Region",?x(2),"Jnl File (def ext: .mjl)"
	w ?x(3),"Before",?x(4),$j("Buff",5),?x(5),$j("Alloc",10)
	w ?x(6),$j("Exten",10),?x(7),$j("AutoSwitch",13)
	w !,BOL,?x(1),$tr($j("",104)," ","-")
	q
seghd:
	s x(0)=32,x(1)=1,x(2)=33,x(3)=53,x(4)=57,x(5)=61,x(6)=67,x(7)=78,x(8)=84
	w !,BOL,!,BOL,?x(0),"*** SEGMENTS ***"
	w !,BOL,?x(1),"Segment",?x(2),"File (def ext: .dat)",?x(3),"Acc",?x(4),"Typ",?x(5),"Block",?x(6),$j("Alloc",10)
	w ?x(7),"Exten",?x(8),"Options"
	w !,BOL,?x(1),$tr($j("",91)," ","-")
	q
maphd:
	s x="*** MAP"_$s($l(mapreg):" for region "_mapreg,1:"")_" ***"
	s mapdispmaxlen=$s(mapdispmaxlen<32:32,1:mapdispmaxlen+2)
	s x(0)=80-$l(x)*.5,x(1)=1,x(2)=x(1)+mapdispmaxlen,x(3)=x(2)+mapdispmaxlen+1
	w !,BOL,!,BOL,?x(0),x
	w !,BOL,?x(1),"  -  -  -  -  -  -  -  -  -  - Names -  -  - -  -  -  -  -  -  -"
	w !,BOL,?x(1),"From",?x(2),"Up to",?x(3),"Region / Segment / File(def ext: .dat)"
	w !,BOL,?x(1),$tr($j("",$s(x(3)=66:122,1:x(3)+38))," ","-")
	q
tmpreghd:
	s x(0)=32,x(1)=1,x(2)=19,x(3)=44,x(4)=50
	s x(5)=58,x(6)=64,x(7)=74,x(8)=79,x(9)=83,x(10)=90,x(11)=96,x(12)=102,x(13)=109,x(14)=115,x(15)=120
	w !,BOL,!,BOL,?x(0),"*** TEMPLATES ***"
	w !,BOL,?x(7),"Std"
	w ?x(9),"Inst"
	w !,BOL
	w ?x(3),$j("Def",4)
	w ?x(4),$j("Rec",7)
	w ?x(5),$j("Key",5)
	w ?x(6),"Null"
	w ?x(7),"Null"
	w ?x(9),"Freeze"
	w ?x(10),"Qdb"
	w ?x(11),"Epoch"
	w ?x(14),"LOCK"
	w !,BOL,?x(1),"Region"
	w ?x(3),$j("Coll",4)
	w ?x(4),$j("Size",7)
	w ?x(5),$j("Size",5)
	w ?x(6),"Subs"
	w ?x(7),"Coll"
	w ?x(8),"Jnl"
	w ?x(9),"on Err"
	w ?x(10),"Rndwn"
	w ?x(11),"Taper"
	w ?x(12),"AutoDB"
	w ?x(13),"Stats"
	w ?x(14),"Crit"
	w !,BOL,?x(1),$tr($j("",118)," ","-")
	q
tmpjnlhd:
	s x(0)=26,x(1)=1,x(2)=18,x(3)=44,x(4)=51,x(5)=57,x(6)=68,x(7)=74
	w !,BOL,?x(2),"Jnl File (def ext: .mjl)"
	w ?x(3),"Before",?x(4),$j("Buff",5),?x(5),$j("Alloc",10)
	w ?x(6),$j("Exten",10),?x(7),$j("AutoSwitch",13)
	w !,BOL,?x(1),$tr($j("",90)," ","-")
	q
tmpseghd:
	s x(0)=32,x(1)=1,x(2)=18,x(3)=38,x(4)=42,x(5)=46,x(6)=52,x(7)=63,x(8)=69
	w !,BOL,!,BOL,?x(1),"Segment",?x(2),"Active",?x(3),"Acc",?x(4),"Typ",?x(5),"Block",?x(6),$j("Alloc",10)
	w ?x(7),"Exten",?x(8),"Options"
	w !,BOL,?x(1),$tr($j("",78)," ","-")
	q
namscollatecalc:
	; we want subscripted names printed in collating order of subscripts (so x(2) gets printed before x(10))
	; whereas they would stored in the "nams" array as nams("x(2)") preceded by nams("x(10)")
	; so determine the gds representation of the names and sort based on that (this automatically includes collation too).
	n s,type,nsubs,gvn,key,coll,keylen,i
	k namscollate
	s s=""
	f  s s=$o(nams(s)) q:'$zl(s)  d
	. s type=$g(nams(s,"TYPE")),nsubs=$g(nams(s,"NSUBS"))
	. i (""=type)!(0=nsubs) s namscollate(s)=s q  ; if not a subscripted name process right away
	. i "POINT"=type s gvn="^"_nams(s,"NAME")
	. e  s gvn="^"_nams(s,"GVNPREFIX")_nams(s,"SUBS",nsubs-1)_")"  ; type="RANGE"
	. s coll=+$g(gnams(nams(s,"SUBS",0),"COLLATION"))
	. s key=$$gvn2gds^GDEMAP(gvn,coll)
	. s key=$ze(key,1,$zl(key)-2)  ; remove trailing 00 00
	. i "RANGE"=type d
	. . ; Some processing needed so X(2,4:5) (a range) comes AFTER X(2,4,5:"") (a point within X(2,4)).
	. . ; Add a 01 at the end of the left subscript of a range.
	. . s key=key_ONE
	. ; ASSERT : i $d(namscollate(key)) s $etrap="zg 0" zsh "*"  zhalt 1  ; assert that checks for duplicate keys
	. s namscollate(key)=s
	q
namedisp(name,addquote)
	; returns a name that is displayable (i.e. if it contains control characters, they are replaced by $c() etc.)
	; if addquote=0, no surrounding double-quotes are added.
	; if addquote=1 and control characters are seen (which will cause _$c(...) to be added)
	;	we will surround string with double-quotes before returning.
	n namezwrlen,namezwr,namedisplen,namedisp,ch,quotestate,starti,i,seenquotestate3
	s namezwr=$zwrite(name) ; this will convert all control characters to $c()/$zc() notation
	; But $zwrite will introduce more double-quotes than we want to display; so remove them
	; e.g. namezwr = "MODELNUM("""_$C(0)_""":"""")"
	s namezwrlen=$zl(namezwr),namedisp="",doublequote=""""
	s namedisp="",namedisplen=0,quotestate=0
	f i=1:1:namezwrlen  s ch=$ze(namezwr,i) d
	. i (quotestate=0) d  q
	. . i (ch=doublequote) s quotestate=1,starti=i+1  q
	. . ; We expect ch to be "$" here
	. . s quotestate=3
	. i (quotestate=1) d  q
	. . i ch'=doublequote q
	. . s quotestate=2  s namedisp=namedisp_$ze(namezwr,starti,i-1),namedisplen=namedisplen+(i-starti),starti=i+1 q
	. i (quotestate=2) d  q
	. . ; At this point ch can be either doublequote or "_"
	. . s quotestate=$s(ch=doublequote:1,1:0)
	. . i ch="_" d  q
	. . . i (($ze(namedisp,namedisplen)'=doublequote)!($ze(namedisp,namedisplen-1)=doublequote)) d  q
	. . . . s starti=(i-1) ; include previous double-quote
	. . . ; remove extraneous ""_ before $c()
	. . . s namedisp=$ze(namedisp,1,namedisplen-1),namedisplen=namedisplen-1,starti=i+1
	. i (quotestate=3) d  q
	. . s seenquotestate3=1
	. . i (ch=doublequote) s quotestate=1 q
	. . i ((ch="_")&($ze(namezwr,i+1,i+3)=(doublequote_doublequote_doublequote))&($ze(namezwr,i+4)'=doublequote))  d  q
	. . . ; remove extraneous _"" after $c()
	. . . s namedisp=namedisp_$ze(namezwr,starti,i-1),namedisplen=namedisplen+(i-starti),starti=i+4,quotestate=1,i=i+3 q
	i addquote&$d(seenquotestate3) s namedisp=doublequote_namedisp_doublequote
	; 2 and 3 are the only terminating states; check that. that too 3 only if addquote is 1.
	; ASSERT : i '((quotestate=2)!(addquote&(quotestate=3))) s $etrap="zg 0" zsh "*"  zhalt 1
	q namedisp
namedisplaycalc:(name)
	; if name is subscripted, make sure control characters are displayed in $c() notation
	n namedisplen,namedisp
	i +$g(nams(name,"NSUBS"))=0 s namsdisp(name)=name q  ; unsubscripted case; return right away
	s namedisp=$$namedisp(name,0)
	s namsdisp(name)=namedisp,namedisplen=$zwidth(namedisp)
	i namedispmaxlen<namedisplen s namedispmaxlen=namedisplen
	q
mapdispcalc:
	n coll,gblname,isplusplus,m,mapdisplen,mlen,mprev,mtmp,name,namedisp,namelen,offset
	s m=""
	f  s mprev=m,m=$o(map(m)) q:'$zl(m)  d
	. i $l(mapreg),(mapreg'=map(m)),('$zl(mprev)!(mapreg'=map(mprev))) q
	. s offset=$zfind(m,ZERO,0)
	. i offset=0  s mapdisp(m)=$tr(m,")","0") q  ; no subscripts case. finish it off first
	. s gblname=$ze(m,1,offset-2),coll=+$g(gnams(gblname,"COLLATION")),mlen=$zl(m)
	. s isplusplus=$$isplusplus^GDEMAP(m,mlen)
	. s mtmp=$s(isplusplus:$ze(m,1,mlen-1),1:m)  ; if ++ type map entry, remove last 01 byte before converting it into gvn
	. s name=$zcollate(mtmp_ZERO_ZERO,coll,1)
	. i isplusplus s name=name_"++"
	. s namelen=$zl(name),name=$ze(name,2,namelen) ; remove '^' at start of name
	. s namedisp=$$namedisp(name,0)
	. s mapdisp(m)=namedisp,mapdisplen=$zwidth(namedisp)
	. i mapdispmaxlen<mapdisplen s mapdispmaxlen=mapdisplen
	q
