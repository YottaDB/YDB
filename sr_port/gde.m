;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
; Copyright (c) 2001-2022 Fidelity National Information		;
; Services, Inc. and/or its subsidiaries. All rights reserved.	;
;								;
; Copyright (c) 2018-2024 YottaDB LLC and/or its subsidiaries.	;
; All rights reserved.						;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
gde:	;base module - d DEBUG^GDE to debug
	n  ; clear calling process M variable state (if any) so it does not interfere with GDE variable names
	s (debug,runtime)=0
DBG:	;transfer point for DEBUG and "runtime" %gde
	; Save parent process context before GDE tampers with it for its own necessities.
	; Most of it is stored in the "gdeEntryState" local variable in subscripted nodes.
	; Exceptions are local collation related act,ncol,nct values which have to be stored in in unsubscripted variables
	;	to prevent COLLDATAEXISTS error as part of the $$set^%LCLCOL below.
	n gdeEditFlag,gdeDev,gdeEntryState,gdeEntryStateAct,gdeEntryStateNcol,gdeEntryStateNct,gdeIOPat,gdeLoopI
	s gdeEntryStateAct=$$get^%LCLCOL
	s gdeEntryStateNcol=$$getncol^%LCLCOL
	s gdeEntryStateNct=$$getnct^%LCLCOL
	; Set local collation to what GDE wants to operate. Errors while doing so will have to exit GDE right away.
	; Prepare special $etrap to issue error in case VIEW "YLCT" call to set local collation fails below
	; Need to use this instead of the gde $etrap (set a few lines later below) as that expects some initialization
	; to have happened whereas we are not yet there since setting local collation is a prerequisite for that init.
	s $et="w !,$p($zs,"","",3,999) s $ecode="""" zm 150503603:$zparse(""$ydb_gbldir"","""",""*.gld"") quit"
	v "YLCT":0:1:0		; sets local variable alternate collation = 0, null collation = 1, numeric collation = 0
	; since GDE creates null subscripts, we don't want user level setting of ydb_lvnullsubs to affect us in any way
	s gdeEntryState("nullsubs")=$v("LVNULLSUBS")
	v "LVNULLSUBS"
	s gdeEntryState("zlevel")=$zlevel-1
	s gdeEntryState("io")=$io
	s $et=$s(debug:"b:$zs'[""%GDE""!allerrs  ",1:"")_"g:(""%GDE%NONAME""[$p($p($zs,"","",3),""-"")) SHOERR^GDE d ABORT^GDE"
	s io=$io,useio="io",comlevel=0,combase=$zl,resume(comlevel)=$zl_":INTERACT"
	i $$set^%PATCODE("M")
	d GDEINIT^GDEINIT,GDEMSGIN^GDEMSGIN,GDFIND^GDESETGD,CREATE^GDEGET:create,LOAD^GDEGET:'create
	; Determine whether GDE is running from a terminal, and if so enable EDITING
	s gdeIOPat=1_""""_io_""""_1_""" "".E1"" TERMINAL "".E" ; pattern to see if a ZSH "D" line is for the current io device
	zsh "d":gdeDev
	s gdeEditFlag=""
	f gdeLoopI=1:1 q:'$d(gdeDev("D",gdeLoopI))  i gdeDev("D",gdeLoopI)?@gdeIOPat s gdeEditFlag="editing:" q
	i debug s prompt="DEBUGDE>",uself="logfile"
	e  d
	. s prompt="GDE>",uself="logfile:(ctrap=$c(3,25,26):exception=""d CTRL^GDE"")"
	. s useio="io:(ctrap=$c(3,25,26):"_gdeEditFlag_"exception=""d CTRL^GDE"")"
	u @useio
	; comline is set to $ZCMDLINE on entry. If the entry zlevel is 0, set the resume point to exit
	i $l(comline) s:'gdeEntryState("zlevel") resume(comlevel)=$zl_":EXIT^GDEEXIT" d comline,EXIT^GDEEXIT
	i runtime s prompt="GD_SHOW>",verb="SHOW",x="" f  s x=$o(syntab(x)) q:'$l(x)  i x'="SHOW" k syntab(x)
INTERACT
	quit:$g(gdequiet)
	f  u io:ctrap=$c(25,26) w !,prompt," " r comline u @useio d comline:$l(comline)
	q
GDELOG
	new $etrap
	set $etrap="w !,$zmessage(gdeerr(""GDELOGFAIL"")),! d GETOUT^GDEEXIT h"
	if $view("YLGDE"),$zauditlog(comline)
	quit
comline:
	do GDELOG
	f cp=1:1 s c=$e(comline,cp) q:(c'=" ")&(c'=TAB)	 ; remove extraneous whitespace at beginning of line
	s ntoken="",ntoktype="TKEOL" s:runtime comline="/"_comline
	d GETTOK^GDESCAN
	i ntoktype="TKEOL" q	 ; if comline begins with a ! don't even bother parsing this line anymore
	i log u @uself w comline,! u @useio
	i runtime n NAME,REGION,SEGMENT,gqual,lquals zg:"/QUIT"[$tr(comline,lower,upper) combase-1 d SHOW^GDEPARSE q
	i ntoktype="TKAT" s resume(comlevel+1)=$zl d comfile q
	d GDEPARSE^GDEPARSE
	q
CTRL
	i $p($zs,",",3,999)["-E-CTRAP, Character trap $C(3) encountered" do  zg @resume(comlevel)
	. i comlevel>0 d comeof ; if we take a ctrl-c in a command file then get out of that command file
	i $p($zs,",",3,999)["-E-CTRAP, Character trap $C(25) encountered" d GETOUT^GDEEXIT h
	i $p($zs,",",3,999)["-E-CTRAP, Character trap $C(26) encountered" d EXIT^GDEEXIT
	i $p($zs,",",3,999)["-E-IOEOF, Attempt to read past an end-of-file" d
	. s $ecode=""	; clear IOEOF condition (not an error) so later GDE can exit with 0 status
	. d comexit
	i $zeof d EXIT^GDEEXIT
	d ABORT
	;
comexit: i 'update d QUIT^GDEQUIT
	i $$ALL^GDEVERIF,$$GDEPUT^GDEPUT s $zstatus=""
	e  w $p($zm(gdeerr("VERIFY")\2*2),"!AD")_"FAILED" w !
	d GETOUT^GDEEXIT
	h
DBGCOMX u $i:exception="" s $et="" d message^GDE(gdeerr("VERIFY"),"""FAILED""") w:'$g(gdequiet) !
	d GETOUT^GDEEXIT
	h
comfile:
	d GETTOK^GDESCAN
	i ntoktype="TKEOL" d message^GDE(gdeerr("QUALREQD"),"""file specification""")
	d TFSPEC^GDEPARSE
	; remove trailing whitespaces in filename
	n i
	f i=$zl(value):-1  s c=$ze(value,i)  q:(c'=" ")&(c'=TAB)  ; remove trailing 0s
	s value=$ze(value,1,i)
	s (comfile,comfile(comlevel+1))=$zparse(value,"","",".COM")
	i '$l($zsearch(comfile)),'$l($zsearch(comfile)) d message^GDE(gdeerr("FILENOTFND"),$zwrite(comfile))
	e  o comfile:(read:exc="zg "_$zl_":comeof") d message^GDE(gdeerr("EXECOM"),$zwrite(comfile)) d SCRIPT
comeof	c comfile s comlevel=$select(comlevel>1:comlevel-1,1:0)
	i comlevel>0 s comfile=comfile(comlevel) d message^GDE(gdeerr("EXECOM"),$zwrite(comfile))
	e  u @useio
	i $p($zs,",",3)'["-E-IOEOF",$p($zs,",",3)'["FILENOTFND" w !,$p($zs,",",3,9999),!
	e  s ($ecode,$zstatus)=""	; clear IOEOF condition (not an error) so later GDE can exit with 0 status
	q
SCRIPT:
	s comlevel=comlevel+1
	f  u comfile r comline u @useio d comline:$l(comline)
	;this loop is terminated by the comfile exception at eof
SHOERR
	w !,$p($zs,",",3,999),!
	s comlevel=$s(comlevel>1:comlevel-1,1:0)
	s $ecode=""
	zg @resume(comlevel)
	q
ABORT
	s abortzs=$zs,abort="GDEDUMP.DMP",$et=""
        o abort:(newversion:noreadonly) u abort zsh "*" c abort
        u @useio
	; make GDECHECK error fatal except native UNIX
        i $d(gdeerr) Write:'$g(gdequiet) $ZMessage($Select((256>abortzs):+abortzs,1:+abortzs\8*8+4)),!
        e  w:'$g(gdequiet) $zs
        d GETOUT^GDEEXIT
	h
DEBUG	;entry point to debug gde
	n  ; clear calling process M variable state (if any) so it does not interfere with GDE variable names
	s allerrs=0,debug=1,runtime=0 u 0:(ctrap="":exception="") zb DBGCOMX,ABORT
	g DBG
message(message,arguments,debug)
	i '$g(gdequiet) n command s command="zm "_message_":"_$g(arguments) x command quit
	s debug=$g(debug)
	;
	; Skip verify OK/Failed messages as we have other ways to know
	; Skip Load of Global directory error messages
	; Skip Global directory update error messages
	i message=gdeerr("VERIFY")!(message=gdeerr("LOADGD"))!(message=gdeerr("GDUPDATE")) quit
	;
	n count,severity
	s count=$i(gdeweberror("count"))
	;
	; Error message structure (from sr_port/err_check.c)
	;  ___________________________________________
	; |     1     FACILITY     1   MSG_IDX     SEV|
	; |___________________________________________|
	;  31   27                 15            3   0
	;
	; severities 0, 2, or 4 indicates that the process should stop
	s severity=message#8
	i severity="0"!(severity="2")!(severity="4"),message>2**27 s gdewebquit=1
	;
	s gdeweberror(count)=$s('debug:$zm(message),1:message)
	;
	; if we have arguments, format the string correctly
	i $l($g(arguments)) d
	. n tokenstart,tokenend,tokenend2,text,argument,fao,j,done
	. s text=""
	. s done=""
	. s argument=1
	. f  q:done  d
	. . ; find the token for replacement
	. . s tokenend=""
	. . s tokenstart=$zf(gdeweberror(count),"!")-1
	. . i tokenstart<0 s done=1 q
	. . i "/_^!"[$ze(gdeweberror(count),tokenstart+1,tokenstart+1) s tokenend=tokenstart+1
	. . e  s tokenend=tokenstart+2
	. . ; if it isn't a valid FAO quit
	. . s fao=$ze(gdeweberror(count),tokenstart,tokenend)
	. . q:'$$isFAO(fao)
	. . ; get all of the text before the token
	. . s text=text_$ze(gdeweberror(count),1,tokenstart-1)
	. . ; There are a few special FAO tokens:
	. . ; !/ Inserts a new line
	. . ; !_ Inserts a tab
	. . ; !^ Inserts a form feed
	. . ; !! Inserts an exclamation point
	. . ;
	. . ; If the FAO token is one of these do the right replacement
	. . i "!/"=fao s text=text_"\n"
	. . e  i "!_"=fao s text=text_$c(9)
	. . e  i "!^"=fao s text=text_"\n" ; pretend a form feed is a new line
	. . e  i "!!"=fao s text=text_"!"
	. . ; concatenate the string with the replacement token
	. . e  s text=text_$zpi(arguments,":",argument) s argument=argument+1
	. . ; set gdeweberror to the rest of the text after the token we replaced
	. . s gdeweberror(count)=$ze(gdeweberror(count),tokenend+1,$zl(gdeweberror(count)))
	. ; pick up the rest of the text
	. s text=text_gdeweberror(count)
	. i $l($g(text)) s gdeweberror(count)=$tr(text,"""","")
	quit
isFAO(string)
	; util_output.c contains the real FAO implementation,
	; this is a simplistic implementation that just does
	; string subsitution
	;
	i string?1"!"1(1"/",1"_",1"^",1"!") QUIT 1
	i string?1"!".N1(1"AC",1"AD",1"AF",1"AS",1"AZ") QUIT 1
	i string?1"!".N1(1"SB",1"SW",1"SL") QUIT 1
	i string?1"!".N1(1"UB",1"UW",1"UL",1"UJ",1"UQ") QUIT 1
	i string?1"!".N1(1"XB",1"XW",1"XL",1"XJ") QUIT 1
	i string?1"!".N1(1"ZB",1"ZW",1"ZL") QUIT 1
	i string?1"!".N1"@"1(1"UJ",1"UQ",1"XJ",1"XQ") QUIT 1
	i string?1"!".N1"*".ANP QUIT 1
	i string?1"!"1"@"1(1"ZJ",1"ZQ") QUIT 1
	QUIT 0