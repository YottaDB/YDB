;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 2001 Sanchez Computer Associates, Inc.	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
gde:	;base module - d DEBUG^GDE to debug
	i $$set^%LCLCOL(0)
	s (debug,runtime)=0
DBG:	;transfer point for DEBUG and "runtime" %gde
	s $zt=$s(debug:"b:$zs'[""%GDE""!allerrs  ",1:"")_"g:(""%GDE%NONAME""[$p($p($zs,"","",3),""-"")) SHOERR^GDE d ABORT^GDE"
	s io=$io,useio="io",comlevel=0,combase=$zl,resume(0)=$zl_":INTERACT"
	i $$set^%PATCODE("M")
	d GDEINIT^GDEINIT,GDEMSGIN^GDEMSGIN,GDFIND^GDESETGD,CREATE^GDEGET:create,LOAD^GDEGET:'create
	i debug s prompt="DEBUGDE>",uself="logfile"
	e  s prompt="GDE>",uself="logfile:(ctrap=$c(3,25,26):exception=""d CTRL^GDE"")"
	e  s useio="io:(ctrap=$c(3,25,26):exception=""d CTRL^GDE"")"
	u @useio
	i $l(comline) d comline,EXIT^GDEEXIT
	i runtime s prompt="GD_SHOW>",verb="SHOW",x="" f  s x=$o(syntab(x)) q:'$l(x)  i x'="SHOW" k syntab(x)
INTERACT
	f  u io:ctrap=$c(25,26) w !,prompt," " r comline u @useio d comline:$l(comline)
	q
comline: s cp=1,ntoken="",ntoktype="TKEOL" s:runtime comline="/"_comline d GETTOK^GDESCAN
	i log u @uself w comline,! u @useio
	i runtime n NAME,REGION,SEGMENT,gqual,lquals zg:"/QUIT"[$tr(comline,lower,upper) combase-1 d SHOW^GDEPARSE q
	i ntoktype="TKAMPER" s resume(comlevel+1)=$zl d comfile q
	d GDEPARSE^GDEPARSE
	q
CTRL
	i $p($zs,",",3,999)["%GTM-E-CTRAP, Character trap $C(3) encountered" s:comlevel comlevel=comlevel-1 zg @resume(comlevel)
	i $p($zs,",",3,999)["%GTM-E-CTRAP, Character trap $C(25) encountered" h
	i $p($zs,",",3,999)["%GTM-E-CTRAP, Character trap $C(26) encountered" d EXIT^GDEEXIT
	i $p($zs,",",3,999)="%GTM-E-IOEOF, Attempt to read past an end-of-file" d comexit
	i $zeof d EXIT^GDEEXIT
	d ABORT
	;
comexit: i 'update d QUIT^GDEQUIT
	i $$ALL^GDEVERIF,$$GDEPUT^GDEPUT  h
DBGCOMX u $i:exception="" s $zt="" zm (gdeerr("VERIFY")\2*2):"FAILED"
	h
comfile:
	d GETTOK^GDESCAN,TFSPEC^GDEPARSE
	s (comfile,comfile(comlevel+1))=$zparse(value,"","",".COM")
	i '$l($zsearch(comfile)),'$l($zsearch(comfile)) zm gdeerr("FNF"):comfile
	e  zm gdeerr("EXECOM"):comfile o comfile:(read:exc="zg "_$zl_":comeof") d SCRIPT
comeof	c comfile s comlevel=comlevel-1
	i comlevel>0 s comfile=comfile(comlevel) zm gdeerr("EXECOM"):comfile
	e  u @useio
	q
SCRIPT:
	s comlevel=comlevel+1
	f  u comfile r comline i $e(comline,1)'="!" u @useio d comline
	;this loop is terminated by the comfile exception at eof
SHOERR
	w !,$p($zs,",",3,999),!
	i comlevel s comlevel=comlevel-1
	zg @resume(comlevel)
	q
ABORT
	s abortzs=$zs,abort="GDEDUMP.DMP",$zt=""
        o abort:(newversion:noreadonly) u abort zsh "*" c abort
        u @useio
        i $d(gdeerr) zm gdeerr("GDECHECK") Write $ZMessage($Select($ZVersion'["VMS"&(256>abortzs):+abortzs,1:+abortzs\8*8+4)),! ; make fatal except native UNIX
        e  w $zs
        h
DEBUG	;entry point to debug gde
	i $$set^%LCLCOL(0)
	s allerrs=0,debug=1,runtime=0 u 0:(ctrap="":exception="") zb DBGCOMX,ABORT
	g DBG
