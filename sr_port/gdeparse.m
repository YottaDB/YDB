;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
; Copyright (c) 2010-2017 Fidelity National Information		;
; Services, Inc. and/or its subsidiaries. All rights reserved.	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
gdeparse:	;command parser
GDEPARSE
	n verb,NAME,GBLNAME,REGION,SEGMENT,gqual,lquals
	d matchtok("TKIDENT","Verb") s verb=token
	d checkkw(.verb,"verb","syntab")
	d @verb
	q
qual(qual,ent,s)
	i ntoktype="TKEOL" zm gdeerr("QUALREQD"):ent
	d matchtok(sep,ent),matchtok("TKIDENT",ent)
	s qual=token d checkkw(.qual,ent,s) s t=@s@(qual)
 	i negated,t'["NEGATABLE" zm gdeerr("NONEGATE"):qual
	i t["REQUIRED",ntoktype'="TKEQUAL",'negated zm gdeerr("VALUEREQD"):qual
	i "NEGATABLE"[t!negated,ntoktype="TKEQUAL" zm gdeerr("NOVALUE"):$s(negated:"NO"_qual,1:qual)
	i t["NEGATABLE" s qual("value")='negated
	i ntoktype="TKEQUAL",t'["LIST" s qual("value")=$$getvalue(s,qual) q
	i ntoktype="TKEQUAL" d list(qual)
	q
getvalue:(s,qual)
	d matchtok("TKEQUAL","Value")
	i ntoktype="TKEOL" zm gdeerr("VALUEREQD"):qual
	d @@s@(qual,"TYPE")
	q value
	;
list:(lhead)
	n listparsing
	s listparsing=TRUE
	s tmp=lqual,v=lqual("value")
	i $ze(comline,cp)="(" d GETTOK^GDESCAN
	n sep
	s sep=ntoktype d getlitm
	i sep="TKLPAREN" s sep=ntoktype f  q:ntoktype="TKRPAREN"  zm:"TKRPAREN|TKCOMMA"'[ntoktype gdeerr("RPAREN") d getlitm
	i ntoktype="TKRPAREN" d GETTOK^GDESCAN
	s lqual=tmp,lqual("value")=v
	q
TNUMBER
	d GETTOK^GDESCAN
	i $l(token)'=$zl(token) zm gdeerr("NONASCII"):token:"number"	; error if the token has non-ascii numbers
	i token'?1.N zm gdeerr("VALUEBAD"):token:"number"
	s value=token
	q
TFSPEC
	n filespec
	i ntoktype="TKSTRLIT" s filespec=ntoken
	e  d TFSPECP
	d GETTOK^GDESCAN
	i $zl(filespec)>(SIZEOF("file_spec")-1) zm gdeerr("VALUEBAD"):filespec:"file specification"
	i '$zl($zparse(filespec,"","","","SYNTAX_ONLY")) zm gdeerr("VALUEBAD"):filespec:"file specification"
	s @("value="_$s($zl(filexfm):filexfm,1:filespec))	; do system specific file name translation
	q
TFSPECP						; scan filespec token by token
	n c,cp1,i				; unix filenames must be quoted to avoid / conflicts with qualifiers
	; first undo any whitespace that was skipped
	f i=1:1 s c=$ze(comline,cp-i) q:(c'=" ")&(c'=TAB)
	s cp1=cp-(i-1)-$zl(ntoken)
	s i=$zl(comline)-cp1+1 ; in Unix any byte is considered acceptable in the file name at the end of the line
	s filespec=$ze(comline,cp1,cp1+i-1),cp=cp1+i
	q
TACCMETH
	d GETTOK^GDESCAN
	i toktype'="TKIDENT" zm gdeerr("VALUEBAD"):token:qual
	s value=$tr(token,lower,upper)
	i '$data(typevalue("STR2NUM","TACCMETH",value)) zm gdeerr("VALUEBAD"):token:qual
	q
TNULLSUB
	d GETTOK^GDESCAN
	i toktype'="TKIDENT" zm gdeerr("VALUEBAD"):token:qual
	s value=$tr(token,lower,upper)
	i '$data(typevalue("STR2NUM","TNULLSUB",value)) zm gdeerr("VALUEBAD"):token:qual
	s value=typevalue("STR2NUM","TNULLSUB",value)
	q
TREGION
	n REGION d REGION s value=REGION
	q
TSEGMENT
	n SEGMENT d SEGMENT s value=SEGMENT
	q
GBLNAME
	k GBLNAME
	n c
	i ntoktype="TKEOL" zm gdeerr("OBJREQD"):"gblname"
	d GETTOK^GDESCAN
	s GBLNAME=token
	i GBLNAME'?1(1"%",1A).AN zm gdeerr("VALUEBAD"):GBLNAME:"gblname"
	i $l(GBLNAME)'=$zl(GBLNAME) zm gdeerr("NONASCII"):GBLNAME:"gblname"		; error if the name is non-ascii
	i $zl(GBLNAME)>PARNAMLN zm gdeerr("VALTOOLONG"):GBLNAME:PARNAMLN:"gblname"
	q
INSTANCE
	i ntoktype="TKEOL" zm gdeerr("OBJREQD"):"instance"
	q
NAME
	k NAME
	n c,len,j,k,tokname,starti,endi,subcnt,nsubs,gblname,type,rangeprefix,nullsub,lsub
	i ntoktype="TKEOL" zm gdeerr("OBJREQD"):"name"
	m nsubs=NAMEsubs	; before GETTOK overwrites it
	s type=NAMEtype		; before GETTOK overwrites it
	d GETTOK^GDESCAN
	s tokname=token
	i "%Y"=$ze(tokname,1,2) zm gdeerr("NOPERCENTY")
	i (MAXGVSUBS<(nsubs-1-$select(type="RANGE":1,1:0))) zm gdeerr("NAMGVSUBSMAX"):tokname:MAXGVSUBS
	; parse subscripted tokname (potentially with ranges) to ensure individual pieces are well-formatted
	; One would be tempted to use $NAME to do automatic parsing of subscripts for well-formedness, but there are issues
	; with it. $NAME does not issue error in various cases (unsubscripted global name longer than 31 characters,
	; numeric subscript mantissa more than 18 digits etc.). And since we want these cases to error out as well, we parse
	; the subscript explicitly below.
	s len=$zl(tokname)
	s j=$g(nsubs(1))
	s gblname=$ze(tokname,1,j-2)
	s NAME=gblname
	i $l(NAME)'=$zl(NAME) zm gdeerr("NONASCII"):NAME:"name"				; error if the name is non-ascii
	s NAME("SUBS",0)=gblname
	i $ze(gblname,j-2)="*" s type="STAR"
	s NAME("TYPE")=type
	i ("*"'=gblname)&(gblname'?1(1"%",1A).AN.1"*") zm gdeerr("VALUEBAD"):gblname:"name"
	i (j-2)>PARNAMLN zm gdeerr("VALTOOLONG"):gblname:PARNAMLN:"name"
	i j=(len+2) s NAME("NSUBS")=0 q  ; no subscripts to process. done.
	; have subscripts to process
	i type="STAR" zm gdeerr("NAMSTARSUBSMIX"):tokname
	i $ze(tokname,len)'=")" zm gdeerr("NAMENDBAD"):tokname
	s NAME=NAME_"("
	s nullsub=""""""
	f subcnt=1:1:nsubs-1 d
	. s k=nsubs(subcnt+1)
	. s sub=$ze(tokname,j,k-2)
	. i (sub="") d
	. . ; allow empty subscripts only on left or right side of range
	. . i (type="RANGE") d
	. . . i (subcnt=(nsubs-2)) s sub=nullsub q  ; if left  side of range is empty, replace with null subscript
	. . . i (subcnt=(nsubs-1)) s sub=nullsub q  ; if right side of range is empty, replace with null subscript
	. i (sub="") zm gdeerr("NAMSUBSEMPTY"):subcnt ; null subscript
	. s c=$ze(sub,1)
	. i (c="""")!(c="$") set sub=$$strsub(sub,subcnt)	; string subscript
	. e  set sub=$$numsub(sub,subcnt)			; numeric subscript
	. i (type="RANGE")&(subcnt=(nsubs-2)) s rangeprefix=NAME,lsub=sub
	. s NAME("SUBS",subcnt)=sub,NAME=NAME_sub,j=k
	. s NAME=NAME_$s(subcnt=(nsubs-1):")",(type="RANGE")&(subcnt=(nsubs-2)):":",1:",")
	s NAME("NSUBS")=nsubs-1,NAME("NAME")=NAME
	i type="RANGE" d
	. ; check if both subscripts are identical; if so morph the RANGE subscript into a POINT type.
	. ; the only exception is if the range is of the form <nullsub>:<nullsub>. In this case, it is actually a range
	. ; meaning every possible value in that subscript.
	. i ((NAME("SUBS",nsubs-1)=lsub)&(lsub'=nullsub)) d  q
	. . s NAME("NAME")=rangeprefix_lsub_")",NAME("NSUBS")=nsubs-2,NAME("TYPE")="POINT",NAME=NAME("NAME")
	. . k NAME("SUBS",nsubs-1)
	. s NAME("GVNPREFIX")=rangeprefix	; subscripted gvn minus the last subscript
	. ; note the below (which does out-of-order check) also does the max-key-size checks for both sides of the range
	. d namerangeoutofordercheck(.NAME,+$g(gnams(gblname,"COLLATION")))
	e  d
	. ; ensure input NAME is within maximum key-size given current gblname value of collation
	. n coll,key
	. s coll=+$g(gblname,"COLLATION")
	. s key=$$gvn2gds^GDEMAP("^"_NAME,coll)
	. d keylencheck(NAME,key,coll)
	q
namerangeoutofordercheck:(nam,coll)
	n rlo,rhi,nsubs,nullsub,rangelo,rangehi,keylo,keyhi,range
	s nullsub=""""""
	s nsubs=nam("NSUBS")
	s rlo=nam("SUBS",nsubs-1),rhi=nam("SUBS",nsubs)
	; if rhi==nullsub then the range is guaranteed to be in order by definition so skip check in that case
	i (rhi'=nullsub) d
	. s range=nam("GVNPREFIX")
	. s rangelo="^"_range_rlo_")",rangehi="^"_range_rhi_")"
	. s keylo=$$gvn2gds^GDEMAP(rangelo,coll),keyhi=$$gvn2gds^GDEMAP(rangehi,coll)
	. d keylencheck(rangelo,keylo,coll)
	. d keylencheck(rangehi,keyhi,coll)
	. i keylo]keyhi zm gdeerr("NAMRANGEORDER"):$$namedisp^GDESHOW(nam("NAME"),0):coll
	q
keylencheck(gvn,key,coll)
	n text
	s text="subscripted name in the database using collation #"_coll
	i $zl(key)>maxreg("KEY_SIZE") zm gdeerr("VALTOOLONG"):gvn:maxreg("KEY_SIZE"):text
	q
gblnameeditchecks(gblname,newcoll)
	; Check if setting collation of "gblname" to "newcoll"
	;	(a) creates out-of-order ranges in existing names
	;	(b) creates range overlaps in existing names
	;	(c) creates subscript representations that exceed the key-size design-maximum (1019)
	;	(d) check if "newcoll" is a valid collation sequence
	; If "gblname" is "*", check all EXISTING name-specifications across all EXISTING global-names.
	;	In this case, "newcoll" is ignored since there is no particular gblname we are interested in.
	n nam,tmpnam,key
	; Test (d)
	d chkcoll(newcoll,gblname)
	; Test (a)
	s nam=""
	f  s nam=$o(nams(nam)) q:'$zl(nam)  d
	. ; for some unsubscripted name specifications (e.g. "*", "#"), nams(nam) might be an unsubscripted node. skip in that case
	. i '$d(nams(nam,"TYPE")) q
	. i (nams(nam,"NSUBS")=0) q  ; if unsubscripted name, then no more checks needed
	. i ("*"'=gblname)&(nams(nam,"SUBS",0)'=gblname) q
	. i gblname="*" s newcoll=+$g(gnams(nams(nam,"SUBS",0),"COLLATION"))
	. i nams(nam,"TYPE")'="RANGE" d  q
	. . ; No need of test (a) since this name is not a range. But do test (c).
	. . ; This takes care of Test (c) for subscripted non-range name specifications.
	. . s key=$$gvn2gds^GDEMAP("^"_nam,newcoll)
	. . d keylencheck(nam,key,newcoll)
	. k tmpnam
	. m tmpnam=nams(nam)
	. s tmpnam=nams(nam)
	. ; The below also takes care of Test (c) for subscripted range name specifications
	. d namerangeoutofordercheck(.tmpnam,newcoll)
	; Test (b)
	i gblname="*" d
	. d namerangeoverlapcheck("")
	e  d namerangeoverlapcheck("","","",gblname,newcoll)
	q
getrangelohikey(nam,keylo,keyhi,coll,range)
	n nsubs,rlo,rhi,nullsub,rlen
	s nullsub=""""""
	s nsubs=nam("NSUBS")
	s rlo=nam("SUBS",nsubs-1),rhi=nam("SUBS",nsubs)
	s rlo="^"_range_rlo_")",keylo=$$gvn2gds^GDEMAP(rlo,coll)
	i (rhi'=nullsub) s rhi="^"_range_rhi_")",keyhi=$$gvn2gds^GDEMAP(rhi,coll)
	e  d
	. ; rhi==nullsub implies max possible subscript at that level which means the lexically next subscript at one higher level
	. s rlen=$zl(range)
	. i $ze(range,rlen)="(" s keyhi=$ze(range,1,rlen-1)_ONE_ZERO_ZERO q
	. s rhi="^"_$ze(range,1,rlen-1)_")",keyhi=$$gvn2gds^GDEMAP(rhi,coll)
	. s rlen=$zl(keyhi),keyhi=$ze(keyhi,1,rlen-2)_ONE_ZERO_ZERO q
	q
namerangeoverlapcheck2:(nam1,reg1,nam2,coll)
	n keylo1,keyhi1,keylo2,keyhi2,range,reg2,maxkey,keylo1inbetween,keyhi1inbetween,overlap
	s reg2=nam2
	s range=nam1("GVNPREFIX")
	i range'=nam2("GVNPREFIX") q  ; if subscripts don't match before the range, there is no chance of a range overlap issue
	i '$data(coll) s coll=+$g(gnams(nam1("SUBS",0),"COLLATION"))
	d getrangelohikey(.nam1,.keylo1,.keyhi1,coll,range)
	d getrangelohikey(.nam2,.keylo2,.keyhi2,coll,range)
	d setinbetween^GDEMAP(keylo1,keylo2,keyhi1,keyhi2,.keylo1inbetween,.keyhi1inbetween)
	; the above sets keylo1inbetween and keyhi1inbetween
	s overlap=0
	i (keylo1inbetween'=keyhi1inbetween) d
	. ; if regions match, no range overlap error needs to be issued but coalesce is needed for sure
	. s overlap=1
	. i reg1=reg2 q  ; if regions match, no need for range overlap error, but need coalesce
	. zm gdeerr("NAMRANGEOVERLAP"):$$namedisp^GDESHOW(nam1("NAME"),0):$$namedisp^GDESHOW(nam2("NAME"),0):coll
	; else check for a few sub-range cases
	e  i (keylo1inbetween) s overlap=1 ; keylo1 and keyhi1 are both in between keylo2 and keyhi2
	; else if keylo2 is in between keylo1 and keyhi1, this means another sub-range case
	e  i ((keylo2=keylo1)!(keylo2]keylo1))&(keyhi1]keylo2) s overlap=1
	; else check if [keylo1,keyhi1] is immediately followed by [keylo2,keyhi2] and map to same region
	e  i (keyhi1=keylo2)&(reg1=reg2) s overlap=1
	; else check if [keylo2,keyhi2] is immediately followed by [keylo1,keyhi1] and map to same region
	e  i (keyhi2=keylo1)&(reg1=reg2) s overlap=1
	; namrangeoverlap array	indicates there are ranges with overlaps mapping to same region
	i (overlap=1) s namrangeoverlap(nam1("NAME"))="",namrangeoverlap(nam2("NAME"))=""
	q
namerangeoverlapcheck(newname,newreg,oldname,gblname,newcoll)
	i (newname'="")&(newname("TYPE")'="RANGE") q  ; if newname is specified and is not a RANGE, there is no overlap possibility
	k namrangeoverlap	; normally we expect this array to be killed once a command completes cleanly
				; but in case of errors, it is possible this exists. In that case, just clean it now.
	n nam1,tmpnam1,nam2,tmpnam2,reg
	s nam1=""
	f  s nam1=$o(nams(nam1)) q:""=nam1  d
	. i nam1=$g(oldname) q  ; if oldname is defined, assume as if that has been deleted from the "nams" array
	. i '$d(nams(nam1,"TYPE")) q  ; for unsubscripted name specifications, "nam1" is an unsubscripted node
	. i nams(nam1,"TYPE")'="RANGE" q
	. i $data(gblname)&(nams(nam1,"SUBS",0)'=gblname) q  ; if called in with a specific gblname, skip other gblname ranges
	. k tmpnam1
	. m tmpnam1=nams(nam1)
	. s tmpnam1=nams(nam1)
	. i newname'="" d namerangeoverlapcheck2(.newname,newreg,.tmpnam1) q
	. s reg=tmpnam1
	. s nam2=""
	. f  s nam2=$o(nams(nam2)) q:nam1=nam2  d
	. . i '$d(nams(nam2,"TYPE")) q  ; for unsubscripted name specifications, "nam2" is an unsubscripted node
	. . i nams(nam2,"TYPE")'="RANGE" q
	. . i $data(gblname)&(nams(nam2,"SUBS",0)'=gblname) q  ; if called in with a specific gblname, skip other gblname ranges
	. . k tmpnam2
	. . m tmpnam2=nams(nam2)
	. . s tmpnam2=nams(nam2)
	. . i '$d(newcoll) d
	. . . d namerangeoverlapcheck2(.tmpnam1,reg,.tmpnam2)
	. . e  d namerangeoverlapcheck2(.tmpnam1,reg,.tmpnam2,newcoll)
	q
chkcoll(coll,gblname,collver)
	i coll=0 q  ; 0 is always a good collation sequence
	i (coll<0)!(coll>maxgnam("COLLATION")) zm gdeerr("GBLNAMCOLLRANGE"):coll
	n savetrap
	s savetrap=$etrap
	n $etrap
	s $etrap="goto collundeferr"
	v "YCHKCOLL":coll
	s $etrap=savetrap
	i $d(collver) d
	. i (0=$view("YCOLLATE",coll,collver)) d
	. . n ver
	. . s ver=$view("YCOLLATE",coll)
	. . i $view("YCOLLATE",coll,ver) zm gdeerr("GBLNAMCOLLVER"):gblname:coll:collver:ver
	q
collundeferr
	i $zstatus'["COLLATIONUNDEF" q  ; don't know how a non-COLLATIONUNDEF error can occur.
					; let parent frame handle this like any other error
	s $ecode=""
	s $etrap=savetrap
	zm gdeerr("GBLNAMCOLLUNDEF"):coll:gblname
	q
strsub:(sub,subcnt)
	n state,xstr,len,iszchar,istart,x,y	; iszchar and istart are initialized in lower level invocations
						; but needed outside that frame too hence the new done here (in parent)
	n retsub	; the subscript that is returned after doing $c() transformations
	n i,previ,doublequote
	; check if string subscript is properly formatted. done using a DFA.
	s state=0,len=$zl(sub),doublequote="""",retsub=doublequote
	f i=1:1:len s c=$ze(sub,i) d @state
	; check if state is terminating
	i (state'=2)&(state'=6) zm gdeerr("NAMNOTSTRSUBS"):subcnt:sub
	i (state=2) s retsub=retsub_$ze(sub,previ,i-1)
	q retsub_doublequote
0	;
	i c=doublequote s state=1,previ=i+1
	e  i c="$" s state=3
	e  zm gdeerr("NAMNOTSTRSUBS"):subcnt:sub
	q
1	;
	i c=doublequote s state=2
	; else state stays at 1
	q
2	;
	i c=doublequote s state=1
	e  i c="_" s state=0,retsub=retsub_$ze(sub,previ,i-2) ; previ would be reset when we execute the label "0" (state=0)
	e  zm gdeerr("NAMNOTSTRSUBS"):subcnt:sub
	q
3	;
	; the only $ functions allowed are $C, $CHAR, $ZCH, $ZCHAR. check for those.
	n j,fn
	s j=$zf(sub,"(",i)
	s fn=$ze(sub,i,$s(j'=0:j-2,1:$zl(sub)))
	s fn=$tr(fn,lower,upper)
	i ((fn="C")!(fn="CHAR")) s iszchar=0
	e  i ((fn="ZCH")!(fn="ZCHAR")) s iszchar=1
	e  zm gdeerr("NAMSTRSUBSFUN"):subcnt:sub
	i j=0 zm gdeerr("NAMSTRSUBSLPAREN"):subcnt:sub	; no "(" found following $
	s i=j-1,state=4
	q
4	;
	s istart=i
	i c'?1N zm gdeerr("NAMSTRSUBSCHINT"):subcnt:sub
	s state=5
	q
5	;
	i c="," d numcheck(istart,i) s state=4 q
	i c=")" d numcheck(istart,i) s state=6 q
	i c'?1N zm gdeerr("NAMSTRSUBSCHINT"):subcnt:sub
	; else state stays at 5
	q
6	;
	i c="_" s state=0
	e  zm gdeerr("NAMNOTSTRSUBS"):subcnt:sub
	q
numcheck(istart,i);
	n num,dollarc
	s num=$ze(sub,istart,i-1)
	d chknumoflow(subcnt,num)
	d chknumexact(subcnt,num,num)
	; check if string subscript has $c() usages. If so, check if $zl($c(NNN)) for each number NNN is non-zero
	i (iszchar&'$zl($zch(num)))!('iszchar&'$zl($c(num))) zm gdeerr("NAMSTRSUBSCHARG"):subcnt:sub:num
	; now that we know $zch()/$c() is passed a valid number, add this to the string subscript to be returned
	s dollarc=$s(iszchar:$zch(num),1:$c(num)),retsub=retsub_dollarc
	i dollarc="""" s retsub=retsub_dollarc	; if double-quote is specified as a $c() expression, use two double-quotes
						; to indicate this is a double-quote inside the string subscript
	q
numsub:(sub,subcnt)
	n mantissa
	; check if a valid subscript. if not error right away
	i sub'?.(.1"+",.1"-").N.1(1".".N).1(1"E"1(.1"+",.1"-")1.N)!(sub=".") zm gdeerr("NAMSUBSBAD"):subcnt:sub
	; check if number too big to be represented in GT.M. If so issue NAMNUMSUBSOFLOW error
	d chknumoflow(subcnt,sub)
	; check if mantissa contains more digits than GT.M can store. If so issue NAMNUMSUBNOTEXACT error
	s mantissa=$p(sub,"E")
	s mantissa=$ztr(mantissa,"-+.")	; remove all -, + and . so we get just the mantissa out
	d chknumexact(subcnt,mantissa,sub)
	; now that we know this is a valid numeric subscript, make it canonical (if needed) e.g. 1E+000 -> 1 etc.
	q +sub
chknumexact(subcnt,mantissa,sub)
	n i,j
	s j=$zl(mantissa)
	f i=1:1:j  q:$ze(mantissa,i)'=0   ; remove leading 0s. keep at least one 0
	i i<j f j=$zl(mantissa):-1  q:$ze(mantissa,j)'=0  ; remove trailing 0s
	s mantissa=$ze(mantissa,i,j)	; this is the real mantissa
	; check if mantissa is non-zero but number too small to be represented in GT.M. If so issue NAMNUMSUBNOTEXACT error
	i (+mantissa)&(0=+sub) zm gdeerr("NAMNUMSUBNOTEXACT"):subcnt:sub
	; check if mantissa cannot be accurately represented in GT.M. If so issue NAMNUMSUBNOTEXACT error
	i +mantissa'=mantissa zm gdeerr("NAMNUMSUBNOTEXACT"):subcnt:sub
	q
chknumoflow(subcnt,sub)
	n $etrap
	s $et="i $zstatus[""NUMOFLOW"" zg -1:numoflowerr"
	s sub=+sub
	q
numoflowerr
	s $ecode=""
	zm gdeerr("NAMNUMSUBSOFLOW"):subcnt:sub
	q
REGION
	k REGION
	i ntoktype="TKEOL" zm gdeerr("OBJREQD"):renpref_"region"
	d GETTOK^GDESCAN
	s REGION=$tr(token,lower,upper)
	i '$zl(REGION) zm gdeerr("VALUEBAD"):token:renpref_"region"
	i $l(REGION)'=$zl(REGION) zm gdeerr("NONASCII"):REGION:"region"		; error if the name of the region is non-ascii
	i REGION=defreg q
	s x=$ze(REGION) i x'?1A d prefixbaderr(REGION,"region")
	i $ze(REGION,2,999)'?.(1AN,1"_",1"$") zm gdeerr("VALUEBAD"):REGION:"region"
	i $zl(REGION)>PARREGLN zm gdeerr("VALTOOLONG"):REGION:PARREGLN:renpref_"region"
	q
SEGMENT
	k SEGMENT
	i ntoktype="TKEOL" zm gdeerr("OBJREQD"):renpref_"segment"
	d GETTOK^GDESCAN
	s SEGMENT=$tr(token,lower,upper)
	i '$zl(SEGMENT) zm gdeerr("VALUEBAD"):token:renpref_"segment"
	i $l(SEGMENT)'=$zl(SEGMENT) zm gdeerr("NONASCII"):SEGMENT:"segment"	; error if the name of the segment is non-ascii
	i SEGMENT=defseg q
	s x=$ze(SEGMENT) i x'?1A d prefixbaderr(SEGMENT,"segment")
	i $ze(SEGMENT,2,999)'?.(1AN,1"_",1"$") zm gdeerr("VALUEBAD"):SEGMENT:"segment"
	i $zl(SEGMENT)>PARSEGLN zm gdeerr("VALTOOLONG"):SEGMENT:PARSEGLN:renpref_"segment"
	q
prefixbaderr:(name,str)
	n namestr
	s namestr="name"
	zm gdeerr("PREFIXBAD"):name:renpref_str:namestr
	q
matchtok:(tok,ent)
	d GETTOK^GDESCAN
	i toktype=tok q
	i tok=sep zm gdeerr("MISSINGDELIM"):tokens(sep):ent:token q
	zm gdeerr("VALUEBAD"):token:ent
	q
checkkw(kw,ent,kwlist)
	n x1,x2
	s kw=$tr(kw,lower,upper)
	i $ze(kw,1,2)="NO" s negated=1,kw=$ze(kw,3,999)
	e  s negated=0
	s x1="" f  s x1=$o(@kwlist@(x1)) q:kw=$ze(x1,1,$zl(kw))!'$zl(x1)
	i '$zl(x1) zm gdeerr("KEYWRDBAD"):kw:ent
	s x2=x1 s x2=$o(@kwlist@(x2))
	i ('$zl(x2))&(kw=$ze(x2,1,$zl(kw))) zm gdeerr("KEYWRDAMB"):kw:ent
	s kw=x1
	q
getqual: d qual(.lqual,"qualifier","syntab("""_verb_""","""_gqual_""")")
	i '$d(lquals(lqual)) s lquals(lqual)=$g(lqual("value"))
	e  zm gdeerr("QUALDUP"):lqual
	q
getlitm: d qual(.lqual,"qualifier","syntab("""_verb_""","""_gqual_""","""_lhead_""")")
	i '$d(lquals(lqual)) s lquals(lqual)=$g(lqual("value"))
	e  zm gdeerr("QUALDUP"):lqual
	q

;-----------------------------------------------------------------------------------------------------------------------------------

ADD
CHANGE
	d qual(.gqual,"object","syntab("""_verb_""")"),@gqual
	f  q:ntoktype="TKEOL"  d getqual
	d @gqual^@("GDE"_$ze(verb,1,5))
	q
RENAME
	n old,new
	d qual(.gqual,"object","syntab("""_verb_""")")
	n renpref
	s renpref="old " d @gqual m old=@gqual	; merge needed since subscripted nodes might also be involved
	s renpref="new " d @gqual m new=@gqual	; merge needed since subscripted nodes might also be involved
	s renpref="" d matchtok("TKEOL","End of line")
	d @gqual^GDERENAM(.old,.new)	; pass by reference since old and new could have subscripted nodes (in case of -NAME)
	q
TEMPLATE
	d qual(.gqual,"object","syntab("""_verb_""")")
	f  q:ntoktype="TKEOL"  d getqual
	d @gqual^GDETEMPL
	q
DELETE
	d qual(.gqual,"object","syntab("""_verb_""")"),@gqual,matchtok("TKEOL","End of line"),@gqual^GDEDELET
	q
LOCKS
	d qual(.gqual,"object","syntab("""_verb_""")"),matchtok("TKEOL","End of line"),LOCKS^GDELOCKS
	q
LOG
	i ntoktype="TKEOL" d INQUIRE^GDELOG q
	d qual(.gqual,"object","syntab("""_verb_""")"),matchtok("TKEOL","End of line"),LOG^GDELOG
	q
SHOW
	i ntoktype="TKEOL" d ALL^GDESHOW q
	d qual(.gqual,"object","syntab("""_verb_""")") s t="|NAME|GBLNAME|REGION|SEGMENT"[("|"_gqual)
	i t,ntoktype="TKEOL" d @("ALL"_$ze(gqual,1,5))^GDESHOW q
	n mapreg
	i gqual="MAP",ntoktype'="TKEOL" d getqual s mapreg=$g(lquals("REGION"))
	i 't,"COMMANDS"=gqual,ntoktype'="TKEOL" d getqual s cfile=$g(lquals("FILE"))
	i 't,"INSTANCE"=gqual,ntoktype="TKEOL" d INSTANCE^GDESHOW q
	d @gqual:t,matchtok("TKEOL","End of line"),@gqual^GDESHOW
	q
VERIFY
	i ntoktype="TKEOL" s x=$$ALL^GDEVERIF q
	d qual(.gqual,"object","syntab("""_verb_""")")
	i "ALL|MAP"[gqual s x=$$ALL^GDEVERIF q
	n verified s verified=1
	i ntoktype="TKEOL" d
	. d @("ALL"_$ze(gqual,1,3))^GDEVERIF
	e  i "NAMEGBLNAMEREGIONSEGMENTINSTANCE"[gqual d
	. d @gqual,@gqual^GDEVERIF
	e  zm gdeerr("NOVALUE"):gqual
	i $d(verified) zm gdeerr("VERIFY"):$s(verified:"OK",1:"FAILED") w !
	q
EXIT
QUIT
	d matchtok("TKEOL","End of line")
	d ^@("GDE"_$tr(verb,lower,upper))
	q
SETGD	f  d  q:ntoktype="TKEOL"
	. d qual(.gqual,"object","syntab("""_verb_""")") s:gqual="FILE" tfile=gqual("value") s:gqual="QUIT" update=0
	d GDESETGD^GDESETGD
	q
HELP
SPAWN
	d ^@("GDE"_$tr(verb,lower,upper))
	q
