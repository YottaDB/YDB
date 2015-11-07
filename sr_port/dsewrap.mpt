;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 2012, 2013 Fidelity Information Services, Inc.    	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%DSEWRAP
%dsewrap
	; Wrappers for DSE to avoid inadvertently making database changes
	set $ETRAP="do errtrap^%DSEWRAP"
	set $ecode=",U255,"	    ; must call an entryref
	quit:$quit 255 quit	    ; in case error trap does not end the call

DUMP(r,o,w,d)	   ; upper case wrapper for dump
	new tmp
	set tmp=$$dump(r,.o,w,d)
	quit:$quit tmp quit

dump(reglist,output,what,detail)	; dump information
	; reglist - comma separated list of regions, "*" (default) for all regions
	; output - required variable passed by reference where output is returned
	; what - optional information to dump - "fileheader" (default) is only option for now
	; detail - optional default is basic information, "all" is full fileheader dump
	; install an error handler if non exists
	if 0=$length($ETRAP) new $ETRAP set $ETRAP="do errtrap^%DSEWRAP"
	new cmd,dse,debug,error,gtmdist,i,io,isVMS,line,mod,nopipe,region,ver
	; save previous $IO to restore once the routine has completed
	set io=$io
	if $data(%DSEWRAP("debug")) do
	.	set debug=$increment(%DSEWRAP("debug"))
	.	set debug(debug,"input","reglist")=$get(reglist)
	.	set debug(debug,"input","what")=$get(what)
	.	set debug(debug,"input","detail")=$get(detail)
	;
	; Determine if we can use PIPES or not AND where to find DSE
	set ver=$tr($piece($zversion," ",2),"V-.",""),ver=$select(ver<1000:ver*10,1:$extract(ver,1,5))
	set isVMS=$zversion["VMS"
	set nopipe=((ver<53003)&($get(%DSEWRAP("forcenopipe"),0)))!isVMS,mod=$select(isVMS:"/",1:"-")
	set gtmdist=$select(isVMS:"gtm$dist",1:"gtm_dist")
	;
	; Applicable regions list
	set reglist=$$reglist($$FUNC^%UCASE($get(reglist,"*")),isVMS)
	; What operation? for now, should be just "dump -fileheader [-all]"
	if (0=$length($get(what)))!("fileheader"=$$FUNC^%LCASE($get(what))) set what=mod_"fileheader"
	else  set $ecode=",U254," quit:$quit 254 quit
	if 0=$length($get(detail)) set detail=""
	else  if "all"=$$FUNC^%LCASE(detail) set detail=mod_"all"
	else  if $length(detail," ")>1 do
	. set detail=$$^%MPIECE(detail," "," ")
	. for i=1:1:$length(detail," ") if $piece(detail," ",i)'?1(1"/",1"-").A set $piece(detail," ",i)=mod_$piece(detail," ",i)
	set cmd="dump "_what_" "_detail
	; Use alternate GT.M version - only supported with pipes
	if nopipe&$data(%DSEWRAP("alternate"))  set $ecode=",U252," quit:$quit 252 quit
	; DSE command to use
	set dse=$get(%DSEWRAP("alternate"),$ztrnlnm(gtmdist)_"/dse")
	if $data(debug) merge debug(debug,"outputreg")=reglist
	if reglist="" set $ecode=",U251," quit:$quit 251 quit
	; Drive DSE
	if 0=nopipe set error=$$dsepipecmd(.output,reglist,dse,cmd)
	else  set error=$$dsefilecmd(.output,reglist,cmd,isVMS)
	use io
	if $data(%DSEWRAP("debug")) merge %DSEWRAP("debug")=debug
	quit:$quit error quit

	; drive DSE through a PIPE device
dsepipecmd(output,regionlist,dse,cmd)
	new error,curreg
	open "dseproc":(shell="/bin/sh":command=dse)::"pipe"
	use "dseproc"
	for curreg=2:1:$length(regionlist,",") do
	. set region=$piece(regionlist,",",curreg)
	. if ($length(region)=0)&($data(debug)) set debug(debug,"reg",curreg)="first"_$c(10)
	. if $length(region)>0 do  ; should be 0 only when curreg=2
	. . write region,! if $data(debug) set debug(debug,"reg",curreg)=region_$c(10)
	. . set error=$$parsefhead(.output,1)
	. write cmd,! if $data(debug) set debug(debug,"reg",curreg)=$get(debug(debug,"reg",curreg))_cmd
	. set error=$$parsefhead(.output,1)
	write "exit",!
	write /eof
	set error=$$parsefhead(.output,1)
	close "dseproc"
	quit error

	; when PIPE devices are not supported drive DSE via a HEREDOC in a script
dsefilecmd(output,regionlist,cmd,isVMS)
	new scriptfile,dsecmd,dumpfile,error,i,hdr,ftr,line,ts
	set ts="_"_$tr($horolog,",","_")
	set scriptfile=$select(isVMS:"dsedump"_ts_".com",1:"dsedump"_ts_".sh")
	set dsecmd=$select(isVMS:"@",1:"chmod 755 "_scriptfile_"; ./")_scriptfile
	set dumpfile="dsedump.txt"
	open scriptfile:newversion use scriptfile
	; print the header
	set hdr=$select(isVMS:"dsecomhdr",1:"dseshhdr")
	for i=1:1  set line=$text(@hdr+i) quit:line["quit"  write $piece(line,";",2),!
	; print the dump commands per region
	for curreg=2:1:$length(regionlist,",") do
	. set region=$piece(regionlist,",",curreg)
	. if $length(region)>0 write region,!  ; should be 0 only when curreg=2
	. write cmd,!
	; print the footer
	set ftr=$select(isVMS:"dsecomftr",1:"dseshftr")
	for i=1:1  set line=$text(@ftr+i) quit:line["quit"  write $piece(line,";",2),!
	; close and execute the script file
	close scriptfile
	zsystem dsecmd
	; read script output and rename the file
	open dumpfile:readonly use dumpfile set error=$$parsefhead(.output)
	close dumpfile:(rename=dumpfile_ts)
	; if no error occurred and debug is UNDEF, delete the script file
	if (0=error)&(0=$data(debug)) open scriptfile:readonly close scriptfile:delete
	quit error

	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	; generate the applicable region list
	;  the output string is a comma separated list of "FIND -REGION=<REGNAME>" and
	;  not a list of regions. The first piece is always null as a null string
	;  indicates that the script could find no applicable regions. The second piece
	;  could be null. If so, that piece represents the first region in which DSE
	;  starts up
reglist(reglist,isVMS)
	; reglist - comma separated list of regions
	; isVMS - use '/' (vms) or '-' (unix) for the modifier
	;	  determine GT.CM regions, '::' (vms) vs ':' (unix)
	new reg,regavail,i,mod,gtcmKey,regpath
	set mod=$select(isVMS:"/",1:"-")
	set gtcmKey=$select(isVMS:"::",1:":")
	; determine the applicable regions - reglist vs actual available regions
	set reglist=$select($get(reglist)="":"*",reglist="ALL":"*",1:reglist)
	set regavail=""
	if "*"'=reglist for i=1:1:$length(reglist,",")  do
	. set reg=$piece(reglist,",",i) set regavail(reg)=1  ; define the region
	kill reg
	for i=1:1 set reg=$view("gvnext",$get(reg)) quit:reg=""  do
	. set regpath=$VIEW("GVFILE",reg)
	. quit:(1<$length(regpath,gtcmKey))
	. if ("*"=reglist)!($data(regavail(reg))) do
	. . set $piece(regavail,",",$length(regavail,",")+1)=$select(i>1:"find "_mod_"region="_reg,1:"")
	quit regavail

	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
parsefhead(output,active)
	new error,debug,fcnt,field,file,i,line,parsed,region,value
	set error=0
	if $data(%DSEWRAP("debug")) set debug=$increment(%DSEWRAP("debug"))
	if $length($etrap)=0 set $etrap="use $p zshow ""*"" halt"
	;
	for i=1:1 read line(i):5 quit:error  quit:$zeof  quit:$select($data(active)=0:0,line(i)["DSE>":1,1:0)  do
	. if ($test=0)&($length(line(i))=0) write !
	. set line=$tr($$FUNC^%TRIM(line(i)),$c(10,13),"")
	. quit:($length(line)=0)
	. if line["%GTM-E-" set error=1 if $data(%DSEWRAP("debug")) set debug(debug,"error")=line
	. if line?1"DSE>".E kill region,file quit			; DSE prompt means reset header information
	. quit:line["Error:  already in region: "			; ignore already in region error
	. if line?1"File"1." ".E set fcnt=$length(line," "),file=$piece(line," ",fcnt) quit
	. if line?1"Region"1." ".E do  quit
	. . set fcnt=$length(line," "),region=$piece(line," ",fcnt)
	. . if $data(parsed(region,"File")) kill parsed(region)		; DUPLICATE, throw it away
	. . set parsed(region,"File")=file
	. quit:$data(region)=0
	. if line?1"Date/Time".E set parsed(region,"Date/Time")=$$FUNC^%TRIM($extract(line,10,$length(line))) quit
	. ;;; Match stats like output "DRD : #"
	. else  if line?3(1U,1N)1" : #".E  do
	. . set field=$$FUNC^%TRIM($piece(line,"0x",1)),value="0x"_$piece(line,"0x",2)
	. . do addfield(.parsed,region,field,value)
	. ;;; Match lines with " : (0x[0-9A0Z]*| (TRUE|FALSE))" in them
	. else  if $length(line," : ")>1 do
	. . set field=$$FUNC^%TRIM($piece(line," : ",1)),value=$tr($piece(line," : ",2)," ","")
	. . do addfield(.parsed,region,field,value)
	. ;;; Match all column oriented data
	. else  do
	. . ;;;; Adjust for the varying column width - the order matters
	. . if $extract(line,44,45)="  " set $extract(line,44,45)="|"		; dump -all : Snapshot information
	. . else  if $extract(line,35,42)="        " set $extract(line,35,42)="|" ; dump -all : all after "Full Block Write.*"
	. . else  if $extract(line,42,43)="  " set $extract(line,42,43)="|"	; dump
	. . new columns,col,data,lastfield,lastpiece
	. . set columns=$length(line,"|")
	. . ;;;; Handle the column data
	. . for col=1:1:columns do
	. . . set data=$$FUNC^%TRIM($piece(line,"|",col))			; trim because of "Snapshot in progress"
	. . . set lastpiece=$length(data," ")
	. . . ;;;;;; Special case - value is a compound statement like "[WAS_ON] OFF"
	. . . if (data["State")&(data["[") set lastpiece=$length(data," ")-1
	. . . ; value is separated by the last space
	. . . set value=$piece(data," ",lastpiece,$length(data," "))
	. . . ; take everything but the value and trim off extra spaces
	. . . set field=$$FUNC^%TRIM($extract(data,1,($length(data)-$length(value))))
	. . . ;;;;;; Special case - fields where value may not exist
	. . . if (data?1"Snapshot file name")!(data?1(1"Journal",1"Temp")1" File:") set value="",field=data
	. . . ;;;;;; Special case - paired records like those with "Transaction ="
	. . . if field="Transaction =" set field=lastfield_" TN"		; handle paired records
	. . . if $data(%DSEWRAP("debug")) set debug(debug,i,field)=value
	. . . do addfield(.parsed,region,field,value)
	. . . set lastfield=field
	merge output=parsed
	if $data(%DSEWRAP("debug")) do
	.	merge %DSEWRAP("debug",debug,"line")=line
	.	set debug(debug,"pipe0ZOEF")=$zeof,debug(debug,"pipe0DEVICE")=$device,debug(debug,"pipe0ZA")=$ZA
	.	merge %DSEWRAP("debug")=debug
	quit:$quit error  quit

addfield(var,region,field,value)
	if $data(var(region,field))\10 set var(region,field,$increment(var(region,field)))=value
	else  if $data(var(region,field)) do
	. set var(region,field,1)=var(region,field),var(region,field)=2,var(region,field,2)=value
	else  set var(region,field)=value
	quit

	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
errtrap
	use $p
	set $etrap="use $principal write $zstatus,! zhalt 1"
	set userecode=$piece($ecode,",",2)
	set errtext=$select(userecode?1"U"3N:$text(@userecode),1:"")
	if $length(errtext) write $text(+0),@$piece(errtext,";",2),!
	else  write $zstatus,!
	if $zlevel<5 zhalt +$extract(userecode,2,$length(userecode))
	quit

	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	; Scripts to drive DSE via ZSystem
dsecomhdr
	;$ define sys$output dsedump.txt
	;$ define sys$error dsedump.txt
	;$ purge /nolog dsedump.txt
	;$ $gtm$dist:dse.exe
	quit
dsecomftr
	;$ deassign sys$output
	;$ deassign sys$error
	quit
dseshhdr
	;#!/bin/sh
	;$gtm_dist/dse > dsedump.txt 2>&1 << EOF
	quit
dseshftr
	;EOF
	quit

	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;	Error message texts
U251	;"-F-NOREGIONS none of the target regions exist"
U252	;"-F-NOPIPENOALTERNATE GT.M "_$zversion_" does not support pipes and cannot drive a different version of GT.M"
U253	;"-F-ILLEGALDETAIL """_detail_""" is not a valid specification of details to dump"
U254	;"-F-ILLEGALSELECTION """_what_""" is not a valid selection of what to dump"
U255	;"-F-BADINVOCATION Must invoke as DO DUMP^"_$text(+0)_"(...)"
