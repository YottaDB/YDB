;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 2001, 2013 Fidelity Information Services, Inc	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
gdescan: ;scanner used by gdeparse
GETTOK
	n c,tmptok,tokisname
	; If -name has been seen, then change GETTOK to fetch next token (which is the actual name specification)
	;   by taking double-quotes into account. Same case with the next TWO tokens (not just one) in case of a
	;   RENAME command where two names are specified after the -name.
	; Otherwise use regular token scanning and search only for delimiters (e.g. " ", "," etc.)
	i ($get(toktype)=sep)&(ntoktype="TKIDENT")&$data(verb)&('$data(gqual))&$data(syntab(verb,"NAME")) d
	. ; check if the current token is -NAME. if so use special rules to parse the next token
	. s tmptok=ntoken
	. d checkkw^GDEPARSE(.tmptok,"object","syntab("""_verb_""")")
	. i tmptok="NAME" s tokisname=1
	e  i ($get(verb)="RENAME")&(($get(toktype)=sep)!$data(gqual)) s tokisname=1
	s token=ntoken,toktype=ntoktype
	; s dbgtoken($incr(dbgtokcnt))=token	; if uncommented, helps debug the GDE token parser
	d skipwhitespace
	s c=$ze(comline,cp)
	i ($c(10)=c)!($c(13)=c) s c="",cp=$zl(comline),ntoktype="TKEOL" d @ntoktype q
	s ntoktype=$s($d(tokens(c)):tokens(c),1:"TKIDENT")
	; If "gqual" is not yet filled in, it means we are still parsing either the "verb" or "gqual" but not "lqual".
	; If we see a double-quote at the start while in this state, parse the token using TKIDENT (and not TKSTRLIT
	; as we dont expect strings in this context).
	i (ntoktype="TKSTRLIT")&('$data(gqual)) s ntoktype="TKIDENT" d TKIDENTspacedelims q
	; Similarly if tokisname is TRUE, then parse the next token as a name-specification
	i $data(tokisname)  d TKIDENTspacedelims q
	d @ntoktype
	q
shotoks: ; for debugging only
	w !,"  toktype: ",toktype,?24," token: '",token,"'"
	w ?48," ntoktype: ",ntoktype,?72,"ntoken: '",ntoken,"'"
	q
skipwhitespace
	n i
	f i=0:1 s c=$ze(comline,cp+i) q:(c'=" ")&(c'=TAB)
	s cp=cp+i
	q
TKIDENT
	; if not parsing a list, a "=" is followed by a token that could have special characters (like "=" or "," or "-" etc.)
	; in this case we dont want these special characters to terminate the parse. Only a whitespace should terminate it.
	; if parsing inside a list, a "," or ")" or "=" could terminate the parse. So we cannot use the whitespace-only parse
	; logic in that case.
	i (toktype="TKEQUAL")&'$data(listparsing) d TKIDENTspacedelims q
	; by similar logic, if "gqual" is not yet filled in, and we did not see a - as the previous token, it means we are
	; parsing the "gqual". In that case, end the parse only when whitespace is encountered, not if "=" or "," is seen.
	i (toktype'=sep)&'$data(gqual) d TKIDENTspacedelims q
	n i
	d tokscan(.tokendelim)
	q
TKIDENTspacedelims
	d tokscan(.spacedelim)
	q
tokscan:(delim)
	n i,c
	i '$data(tokisname) d
	. f i=0:1 s c=$ze(comline,cp+i) q:$data(delim(c))
	e  d
	. ; About to parse the token following a -name. Take double-quotes into account.
	. ; Any delimiter that comes inside a double-quote does NOT terminate the scan/parse.
	. ; Implement the following DFA (Deterministic Finite Automaton)
	. ;	  State 0 --> next char is     a double-quote --> State 1
	. ;	  State 0 --> next char is NOT a double-quote --> State 0
	. ;	  State 1 --> next char is     a double-quote --> State 2
	. ;	  State 1 --> next char is NOT a double-quote --> State 1
	. ;	  State 2 --> next char is     a double-quote --> State 1
	. ;	  State 2 --> next char is NOT a double-quote --> State 0
	. ; Also note down (in NAMEsubs) the columns where LPAREN, COMMA and COLON appear. Later used in NAME^GDEPARSE
	. n quotestate,parenstate,errstate,quitloop
	. s quotestate=0,parenstate=0,errstate=""
	. k NAMEsubs ; this records the column where subscript delimiters COMMA or COLON appear in the name specification
	. k NAMEtype
	. s NAMEtype="POINT",NAMEsubs=0,quitloop=0
	. f i=0:1 s c=$ze(comline,cp+i) q:(c="")  d  q:quitloop
	. . i c="""" s quotestate=$s(quotestate=1:2,1:1)
	. . e        s quotestate=$s(quotestate=2:0,1:quotestate) i 'quotestate d
	. . . i $data(delim(c)) s quitloop=1 q
	. . . i (parenstate=2) i '$zl(errstate) s errstate="NAMRPARENNOTEND"
	. . . i (c="(") d
	. . . . i parenstate s parenstate=parenstate+2  q   ; nested parens
	. . . . s parenstate=1
	. . . . s NAMEsubs($incr(NAMEsubs))=(i+2)
	. . . i (c=",") d
	. . . . i 'parenstate i '$zl(errstate) s errstate="NAMLPARENNOTBEG"
	. . . . i (1'=parenstate) q   ; nested parens
	. . . . i NAMEtype="RANGE" i '$zl(errstate) s errstate="NAMRANGELASTSUB"
	. . . . s NAMEsubs($incr(NAMEsubs))=(i+2)
	. . . i c=":" d
	. . . . i 'parenstate i '$zl(errstate) s errstate="NAMLPARENNOTBEG"
	. . . . i NAMEtype="RANGE" i '$zl(errstate) s errstate="NAMONECOLON"
	. . . . s NAMEsubs($incr(NAMEsubs))=(i+2),NAMEtype="RANGE"
	. . . i c=")" d
	. . . . i 'parenstate i '$zl(errstate) s errstate="NAMLPARENNOTBEG"
	. . . . i (1'=parenstate) s parenstate=parenstate-2 q   ; nested parens
	. . . . s parenstate=2
	. . . . s NAMEsubs($incr(NAMEsubs))=(i+2)
	. i quotestate i '$zl(errstate) s errstate="STRMISSQUOTE"
	. i (1=parenstate)!(2<parenstate) i '$zl(errstate) s errstate="NAMRPARENMISSING"
	. i $zl(errstate) zm gdeerr(errstate):$ze(comline,cp,cp+i-1)
	. i 'NAMEsubs s NAMEsubs($incr(NAMEsubs))=i+2
	i c="" d
	. ; check if tail of last token in line contains $c(13,10) and if so remove it
	. ; this keeps V61 GDE backward compatible with V60 GDE
	. n j
	. f j=1:1 s c=$ze(comline,cp+i-j) q:($c(10)'=c)&($c(13)'=c)
	. s i=i-j+1
	s ntoken=$ze(comline,cp,cp+i-1),cp=cp+i
	d skipwhitespace
	i (ntoken="!") d TKEXCLAM	; if found a ! instead of a TKIDENT type token, set ntoktype to TKEOL
	q
TKSTRLIT
	n i,len
	s len=$zl(comline)
	f i=1:1:(len-cp) q:$ze(comline,cp+i)=""""
	i (i=(len-cp))&($ze(comline,cp+i)'="""") zm gdeerr("STRMISSQUOTE"):$ze(comline,cp,cp+i)
	s ntoken=$ze(comline,cp+1,cp+i-1),cp=cp+i+1
	d skipwhitespace
	q
TKAT
TKCOMMA
TKDASH ; see below for more UNIXy alternative
TKEQUAL
TKLPAREN
TKRPAREN
TKSLASH
	s ntoken=c,cp=cp+1
	i (ntoktype="TKRPAREN") d skipwhitespace
	q
TKEXCLAM
	s ntoktype="TKEOL"
	s ntoken=""
	s cp=$zl(comline)
	q
;TKDASH - more UNIXy handling disabled for compatibility with other utilities
	s ntoken=c,cp=cp+1
	i sep="TKDASH",$ze(comline,cp)?1A s c=$ze(comline,cp-2) i c=" "!(c=TAB) q
	zm gdeerr("ILLCHAR"):"-"
	q
TKEOL
	s ntoken=""
	q
