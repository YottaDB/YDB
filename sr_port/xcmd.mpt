;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright  2012 Fidelity Information Services, Inc.    	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	; Utility to execute a shell command and return a non-zero status on error
	;
%XCMD	; Usage: mumps -run %XCMD '<string>'
	;
	set $ETRAP="goto CLIERR^%XCMD"
	do  ; protect %XCMD's error handler
	. new $ETRAP set $ETRAP="goto CLIERR^%XCMD"
	. xecute $zcmdline
	quit

CLIERR
	for i=1:1:$length($zstatus,",")  quit:i>$length($zstatus,",")  do
	. if $piece($zstatus,",",i)?1(1"%",1"-")3.U1"-"1U1"-".E do 	; split on GTM error format
	. . write:$data(lasti) $piece($zstatus,",",lasti,i-1),!
	. . set lasti=i
	write $piece($zstatus,",",$get(lasti,3),i),!
	zhalt +$piece($zstatus,",",1)
	quit

	; Perform a given command on every line of input
	;
LOOP	; Usage: mumps -run LOOP^%XCMD [--before=|<string>|] [--after=|<string>|] --xec=|<string>|
	set $ETRAP="do LOOPERR^%XCMD"
	new %cli,%l,%NR,%xcmd
	set %cli=$zcmdline
	for  quit:'$$trimleadingstr(.%cli,"--")  do 	; process command line options
	. if $$trimleadingstr(.%cli,"after=") set %xcmd("after")=$$trimleadingdelimstr(.%cli)
	. else  if $$trimleadingstr(.%cli,"before=") set %xcmd("before")=$$trimleadingdelimstr(.%cli)
	. else  if $$trimleadingstr(.%cli,"xec=") set %xcmd("xec")=$$trimleadingdelimstr(.%cli)
	. else  set $ecode=",U254,"
	. if $$trimleadingstr(.%cli," ")
	set:'$length($get(%xcmd("xec"))) $ecode=",U253,"
	set:$length(%cli) $ecode=",U252,"
	kill %cli
	do  ; protect %XCMD's error handler
	. new $ETRAP set $ETRAP="do LOOPERR^%XCMD"
	. do cmd($get(%xcmd("before")),0,"")
	. for %NR=1:1 read %l quit:$zeof  do cmd(%xcmd("xec"),%NR,%l)
	. do cmd($get(%xcmd("after")),%NR,%l)
	quit

cmd(cmd,%NR,%l)
	quit:$length(cmd)=0
	new %xcmd
	xecute cmd
	quit

LOOPERR
	; attempt to trap internal errors
	set uecode=$piece($ecode,",",2),uemsg=$text(@uecode)
	if $length(uemsg) write $text(+0),@$piece(uemsg,";",2),! zhalt +$extract(uecode,2,$length(uecode))
	use $principal
	write $zstatus,!
	zhalt 1
	quit

	; Remove and optionally return leading delimited string from str
trimleadingdelimstr(str)
	new delim,substr
	set delim=$extract(str,1)
	set substr=$piece(str,delim,2)
	set str=$extract(str,$length(substr)+3,$length(str))
	quit:$quit substr quit

	; Remove and optionally return first piece of s with space as piece separator
trimleadingpiece(str)
	new tmp
	set tmp=$piece(str," ",1)
	set str=$piece(str," ",2,$length(str," "))
	quit:$quit tmp quit

	; Return s without leading $length(x) characters; return 1/0 if called as function
trimleadingstr(str,x)
	if x=$extract(str,1,$length(x)) set str=$extract(str,$length(x)+1,$length(str)) quit:$quit 1 quit
	else  quit:$quit 0 quit

;	Error message texts
U252	;"-F-UNRECCMD Unrecognized commands starting with "_%cli
U253	;"-F-EMPTYXEC String to Xecute with --xec is required but not provided"
U254	;"-F-ILLEGALCMD Illegal command line option(s) starting with --"_%cli
