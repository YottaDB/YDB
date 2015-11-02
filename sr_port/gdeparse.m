;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 2010 Fidelity Information Services, Inc	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
gdeparse:	;command parser
GDEPARSE
	n verb,NAME,REGION,SEGMENT,gqual,lquals
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
	i "NEGATABLE"[t!negated,ntoktype="TKEQUAL" zm gdeerr("NOVALUE"):qual
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
	n sep
	s tmp=lqual,v=lqual("value")
	i $e(comline,cp)="(" d GETTOK^GDESCAN
	s sep=ntoktype d getlitm
	i sep="TKLPAREN" s sep=ntoktype f  q:ntoktype="TKRPAREN"  zm:"TKRPAREN|TKCOMMA"'[ntoktype gdeerr("RPAREN") d getlitm
	i ntoktype="TKRPAREN" d GETTOK^GDESCAN
	s lqual=tmp,lqual("value")=v
	q
TNUMBER
	d GETTOK^GDESCAN
	i toktype'="TKNUMLIT" zm gdeerr("VALUEBAD"):token:"number"
	i $l(token)'=$zl(token) zm gdeerr("NONASCII"):token:"number"			; error if the token has non-ascii numbers
	s value=token
	q
TFSPEC
	k filespec
	i ntoktype="TKEOL" zm gdeerr("QUALREQD"):"file specification"
	i ntoktype="TKSTRLIT" s filespec=$ze(ntoken,2,$zl(ntoken)-1)
	e  d TFSPECP
 	d GETTOK^GDESCAN			; put the scanner back on track
	i $zl(filespec)>(SIZEOF("file_spec")-1) zm gdeerr("VALUEBAD"):filespec:"file specification"
	i '$l($zparse(filespec,"","","","SYNTAX_ONLY")) zm gdeerr("VALUEBAD"):filespec:"file specification"
	s @("value="_$s($l(filexfm):filexfm,1:filespec))	; do system specific file name translation
	q
TFSPECP						; scan filespec token by token
	n c,cp1					; unix filenames must be quoted to avoid / conflicts with qualifiers
	s cp1=cp-$l(ntoken)
	f i=0:1 s c=$e(comline,cp1+i) q:c'?@dbfilpar!'$l(c)
	s filespec=$e(comline,cp1,cp1+i-1),cp=cp1+i
	q
TACCMETH
	d GETTOK^GDESCAN
	i toktype'="TKIDENT" zm gdeerr("VALUEBAD"):token:qual
	s value=$tr(token,lower,upper)
	i @s@(qual,"TYPE","VALUES")'[("\"_value) zm gdeerr("VALUEBAD"):token:qual
	q
TNULLSUB
	d GETTOK^GDESCAN
	i toktype'="TKIDENT" zm gdeerr("VALUEBAD"):token:qual
	s value=$tr(token,lower,upper)
	i @s@(qual,"TYPE","VALUES")'[("\"_value) zm gdeerr("VALUEBAD"):token:qual
	q
TREGION
	n REGION d REGION s value=REGION
	q
TSEGMENT
	n SEGMENT d SEGMENT s value=SEGMENT
	q
NAME
	k NAME
	i ntoktype="TKEOL" zm gdeerr("OBJREQD"):"name"
	n c,cp1
	s cp1=cp-$l(ntoken)
	f i=0:1 s c=$e(comline,cp1+i) q:c'?.1"%".1AN.1"*"!'$l(c)
	s NAME=$e(comline,cp1,cp1+i-1),cp=cp1+i d GETTOK^GDESCAN			; put the scanner back on track
	i '$l(NAME) zm gdeerr("VALUEBAD"):token:"name"
	i $l(NAME)'=$zl(NAME) zm gdeerr("NONASCII"):NAME:"name"				; error if the name is non-ascii
	i NAME'="*" s x=$e(NAME) i x'="%",x'?1A zm gdeerr("NAMSTARTBAD"):NAME
	i $e(NAME,2,999)'?.AN.1"*" zm gdeerr("VALUEBAD"):NAME:"name"
	i $l(NAME)>PARNAMLN zm gdeerr("VALTOOLONG"):NAME:PARNAMLN:"name"
	q
REGION
	k REGION
	i ntoktype="TKEOL" zm gdeerr("OBJREQD"):renpref_"region"
	n c,cp1
	s cp1=cp-$l(ntoken)
	f i=0:1 s c=$e(comline,cp1+i) q:c'?.1AN.1"$".1"_"!'$l(c)
	s REGION=$tr($e(comline,cp1,cp1+i-1),lower,upper),cp=cp1+i d GETTOK^GDESCAN		; put the scanner back on track
	i '$l(REGION) zm gdeerr("VALUEBAD"):token:renpref_"region"
	i $l(REGION)'=$zl(REGION) zm gdeerr("NONASCII"):REGION:"region"		; error if the name of the region is non-ascii
	i REGION=defreg q
	s x=$e(REGION) i x'?1A zm gdeerr("PREFIXBAD"):REGION:renpref_"region"
	i $l(REGION)>PARREGLN zm gdeerr("VALTOOLONG"):REGION:PARREGLN:renpref_"region"
	q
SEGMENT
	k SEGMENT
	i ntoktype="TKEOL" zm gdeerr("OBJREQD"):renpref_"segment"
	n c,cp1
	s cp1=cp-$l(ntoken)
	f i=0:1 s c=$e(comline,cp1+i) q:c'?.1AN.1"$".1"_"!'$l(c)
	s SEGMENT=$tr($e(comline,cp1,cp1+i-1),lower,upper),cp=cp1+i d GETTOK^GDESCAN		; put the scanner back on track
	i '$l(SEGMENT) zm gdeerr("VALUEBAD"):token:renpref_"segment"
	i $l(SEGMENT)'=$zl(SEGMENT) zm gdeerr("NONASCII"):SEGMENT:"segment"	; error if the name of the segment is non-ascii
	i SEGMENT=defseg q
	s x=$e(SEGMENT) i x'?1A zm gdeerr("PREFIXBAD"):SEGMENT:renpref_"segment"
	i $l(SEGMENT)>PARSEGLN zm gdeerr("VALTOOLONG"):SEGMENT:PARSEGLN:renpref_"segment"
	q
matchtok:(tok,ent)
	d GETTOK^GDESCAN
	i toktype=tok q
	zm gdeerr("VALUEBAD"):token:ent
	q
checkkw:(kw,ent,kwlist)
	n x1,x2
	s kw=$tr(kw,lower,upper)
	i $e(kw,1,2)="NO" s negated=1,kw=$e(kw,3,999)
	e  s negated=0
	s x1="" f  s x1=$o(@kwlist@(x1)) q:kw=$e(x1,1,$l(kw))!'$l(x1)
	i '$l(x1) zm gdeerr("KEYWRDBAD"):kw:ent
	s x2=x1 f  s x2=$o(@kwlist@(x2)) q:kw=$e(x2,1,$l(kw))!'$l(x2)
	i $l(x2) zm gdeerr("KEYWRDAMB"):kw:ent
	s kw=x1
	q
getqual: d qual(.lqual,"Local qualifier","syntab("""_verb_""","""_gqual_""")")
	i '$d(lquals(lqual)) s lquals(lqual)=$g(lqual("value"))
	e  zm gdeerr("QUALDUP"):lqual
	q
getlitm: d qual(.lqual,"Local qualifier","syntab("""_verb_""","""_gqual_""","""_lhead_""")")
	i '$d(lquals(lqual)) s lquals(lqual)=$g(lqual("value"))
	e  zm gdeerr("QUALDUP"):lqual
	q

;-----------------------------------------------------------------------------------------------------------------------------------

ADD
CHANGE
	d qual(.gqual,"Global qualifier","syntab("""_verb_""")"),@gqual
	f  q:ntoktype="TKEOL"  d getqual
	d @gqual^@("GDE"_$e(verb,1,5))
	q
RENAME
	d qual(.gqual,"Global qualifier","syntab("""_verb_""")")
	n renpref
	s renpref="old " d @gqual s old=@gqual
	s renpref="new " d @gqual s new=@gqual
	s renpref="" d matchtok("TKEOL","End of line")
	d @gqual^GDERENAM(old,new)
	q
TEMPLATE
	d qual(.gqual,"Global qualifier","syntab("""_verb_""")")
	f  q:ntoktype="TKEOL"  d getqual
	d @gqual^GDETEMPL
	q
DELETE
	d qual(.gqual,"Global qualifier","syntab("""_verb_""")"),@gqual,matchtok("TKEOL","End of line"),@gqual^GDEDELET
	q
LOCKS
	d qual(.gqual,"Global qualifier","syntab("""_verb_""")"),matchtok("TKEOL","End of line"),LOCKS^GDELOCKS
	q
LOG
	i ntoktype="TKEOL" d INQUIRE^GDELOG q
	d qual(.gqual,"Global qualifier","syntab("""_verb_""")"),matchtok("TKEOL","End of line"),LOG^GDELOG
	q
SHOW
	i ntoktype="TKEOL" d ALL^GDESHOW q
	d qual(.gqual,"Global qualifier","syntab("""_verb_""")") s t="NAMEREGIONSEGMENT"[gqual
	i t,ntoktype="TKEOL" d @("ALL"_$e(gqual,1,5))^GDESHOW q
	n mapreg
	i gqual="MAP",ntoktype'="TKEOL" d getqual s mapreg=$g(lquals("REGION"))
	i 't,"COMMANDS"=gqual,ntoktype'="TKEOL" d getqual s cfile=$g(lquals("FILE"))
	d @gqual:t,matchtok("TKEOL","End of line"),@gqual^GDESHOW
	q
VERIFY
	i ntoktype="TKEOL" s x=$$ALL^GDEVERIF q
	d qual(.gqual,"Global qualifier","syntab("""_verb_""")")
	i "ALL|MAP"[gqual s x=$$ALL^GDEVERIF q
	n verified s verified=1
	i ntoktype="TKEOL" d @("ALL"_$e(gqual,1,3))^GDEVERIF i 1
	e  i "NAMEREGIONSEGMENT"[gqual d @gqual,@gqual^GDEVERIF i 1
	e  zm gdeerr("NOVALUE"):gqual
	i $d(verified) zm gdeerr("VERIFY"):$s(verified:"OK",1:"FAILED") w !
	q
EXIT
QUIT
	d matchtok("TKEOL","End of line")
	d ^@("GDE"_$tr(verb,lower,upper))
	q
SETGD	f  d  q:ntoktype="TKEOL"
	. d qual(.gqual,"Global qualifier","syntab("""_verb_""")") s:gqual="FILE" tfile=gqual("value") s:gqual="QUIT" update=0
	d GDESETGD^GDESETGD
	q
HELP
SPAWN
	d ^@("GDE"_$tr(verb,lower,upper))
	q
