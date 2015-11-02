;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 2012 Fidelity Information Services, Inc.    	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%DSEWRAP
	; Wrappers for DSE to avoid inadvertently making database changes
	set $ztrap="" do errtrap    ; set up error trap
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

	set $ztrap="" do errtrap
	new allregs,cmd,file,i,io,line,region,timestamp,tmp1,tmp2,tmp3,tmp4,tmp5
	set io=$io,timestamp=$horolog
	set (allregs,tmp1)=$view("gvnext","") for  set tmp1=$view("gvnext",tmp1) quit:'$length(tmp1)  set allregs=allregs_","_tmp1
	set:'$length($get(reglist))!("*"=$get(reglist)) reglist=allregs
	set reglist=","_$$FUNC^%UCASE(reglist)_","
	if '$length($get(what))!("fileheader"=$$FUNC^%LCASE(what)) set what=" -fileheader"
	else  set $ecode=",U254," quit:$quit 254 quit
	if '$length($get(detail)) set detail=""
	else  if "all"=$$FUNC^%LCASE(detail) set detail=" -all"
	else  set $ecode=",U253," quit:$quit 253 quit
	set cmd="dump"_what_detail
	open "dseproc":(shell="/bin/sh":command=$ztrnlnm("gtm_dist")_"/dse":stderr="dseprocstderr")::"pipe"
	use "dseproc" write:reglist[(","_$piece(allregs,",",1)_",") cmd,!
		for i=2:1:$length(allregs,",") set tmp1=$piece(allregs,",",i) write:reglist[(","_tmp1_",") "find -region=",tmp1,!,cmd,!
	write "exit",!
	write /eof
	use "dseprocstderr" for i=1:1 read line quit:$zeof  do
	. do:$length($$trimwhsp(line)) ; Ad hoc rules for processing DSE stderr
	. . if $length(line,$char(9))>1 set:"Region"=$piece(line,$char(9),1) region=$piece(line,$char(9),2)
	. . else  if "Date/Time"=$extract(line,1,9) set output($zgbldir,region,timestamp,"Date/Time")=$$trimwhsp($extract(line,10,$length(line)))
	. . else  if ":"=$extract(line,7) do
	. . . set tmp1=$$trimwhsp(line),tmp2=$length(tmp1," ")
	. . . set tmp4=$$trimendwhsp($piece(tmp1," ",1,tmp2-1)),tmp5=$$trimbegwhsp($piece(tmp1," ",tmp2))
	. . . if $data(output($zgbldir,region,timestamp,tmp4))\10 set output($zgbldir,region,timestamp,tmp4,$increment(output($zgbldir,region,timestamp,tmp4)))=tmp5
	. . . else  if $data(output($zgbldir,region,timestamp,tmp4)) set output($zgbldir,region,timestamp,tmp4,1)=output($zgbldir,region,timestamp,tmp4),output($zgbldir,region,timestamp,tmp4)=2,output($zgbldir,region,timestamp,tmp4,2)=tmp5
	. . . else  set output($zgbldir,region,timestamp,tmp4)=tmp5
	. . else  if $length(line)>44&(" "=$extract(line,1))&(" "=$extract(line,44)&(":"'=$extract(line,45))) do
	. . . set tmp1=$select(0=$extract(line,45):45,1:43),tmp2=$$trimwhsp($extract(line,1,tmp1)),tmp3=$length(tmp2," ")
	. . . set tmp4=$$trimendwhsp($piece(tmp2," ",1,tmp3-1)),tmp5=$$trimbegwhsp($piece(tmp2," ",tmp3))
	. . . if $data(output($zgbldir,region,timestamp,tmp4))\10 set output($zgbldir,region,timestamp,tmp4,$increment(output($zgbldir,region,timestamp,tmp4)))=tmp5
	. . . else  if $data(output($zgbldir,region,timestamp,tmp4)) set output($zgbldir,region,timestamp,tmp4,1)=output($zgbldir,region,timestamp,tmp4),output($zgbldir,region,timestamp,tmp4)=2,output($zgbldir,region,timestamp,tmp4,2)=tmp5
	. . . else  set output($zgbldir,region,timestamp,tmp4)=tmp5
	. . . set tmp2=$$trimwhsp($extract(line,tmp1+1,$length(line))),tmp3=$length(tmp2," ")
	. . . set tmp4=$$trimendwhsp($piece(tmp2," ",1,tmp3-1)),tmp5=$$trimbegwhsp($piece(tmp2," ",tmp3))
	. . . if $data(output($zgbldir,region,timestamp,tmp4))\10 set output($zgbldir,region,timestamp,tmp4,$increment(output($zgbldir,region,timestamp,tmp4)))=tmp5
	. . . else  if $data(output($zgbldir,region,timestamp,tmp4)) set output($zgbldir,region,timestamp,tmp4,1)=output($zgbldir,region,timestamp,tmp4),output($zgbldir,region,timestamp,tmp4)=2,output($zgbldir,region,timestamp,tmp4,2)=tmp5
	. . . else  set output($zgbldir,region,timestamp,tmp4)=tmp5
	. . else  if $length(line)>45&(" "=$extract(line,1))&(" "=$extract(line,45)) do
	. . . set tmp1=$select(" "=$extract(line,46):45,1:44),tmp2=$$trimwhsp($extract(line,1,tmp1)),tmp3=$length(tmp2," ")
	. . . set tmp4=$$trimendwhsp($piece(tmp2," ",1,tmp3-1)),tmp5=$$trimbegwhsp($piece(tmp2," ",tmp3))
	. . . if $data(output($zgbldir,region,timestamp,tmp4))\10 set output($zgbldir,region,timestamp,tmp4,$increment(output($zgbldir,region,timestamp,tmp4)))=tmp5
	. . . else  if $data(output($zgbldir,region,timestamp,tmp4)) set output($zgbldir,region,timestamp,tmp4,1)=output($zgbldir,region,timestamp,tmp4),output($zgbldir,region,timestamp,tmp4)=2,output($zgbldir,region,timestamp,tmp4,2)=tmp5
	. . . else  set output($zgbldir,region,timestamp,tmp4)=tmp5
	. . . set tmp2=$$trimwhsp($extract(line,tmp1+1,$length(line))),tmp3=$length(tmp2," ")
	. . . set tmp4=$$trimendwhsp($piece(tmp2," ",1,tmp3-1)),tmp5=$$trimbegwhsp($piece(tmp2," ",tmp3))
	. . . if $data(output($zgbldir,region,timestamp,tmp4))\10 set output($zgbldir,region,timestamp,tmp4,$increment(output($zgbldir,region,timestamp,tmp4)))=tmp5
	. . . else  if $data(output($zgbldir,region,timestamp,tmp4)) set output($zgbldir,region,timestamp,tmp4,1)=output($zgbldir,region,timestamp,tmp4),output($zgbldir,region,timestamp,tmp4)=2,output($zgbldir,region,timestamp,tmp4,2)=tmp5
	. . . else  set output($zgbldir,region,timestamp,tmp4)=tmp5
	. . else  do
	. . . set tmp1=$$trimwhsp(line),tmp2=$length(tmp1," ")
	. . . set tmp4=$$trimendwhsp($piece(tmp1," ",1,tmp2-1)),tmp5=$$trimbegwhsp($piece(tmp1," ",tmp2))
	. . . if $data(output($zgbldir,region,timestamp,tmp4))\10 set output($zgbldir,region,timestamp,tmp4,$increment(output($zgbldir,region,timestamp,tmp4)))=tmp5
	. . . else  if $data(output($zgbldir,region,timestamp,tmp4)) set output($zgbldir,region,timestamp,tmp4,1)=output($zgbldir,region,timestamp,tmp4),output($zgbldir,region,timestamp,tmp4)=2,output($zgbldir,region,timestamp,tmp4,2)=tmp5
	. . . else  set output($zgbldir,region,timestamp,tmp4)=tmp5
	use io close "dseproc"
	set region="" for  set region=$order(output($zgbldir,region)) quit:'$length(region)  do	; cleanup post processing
	. set tmp1="" for  set tmp1=$order(output($zgbldir,region,timestamp,tmp1)) quit:'$length(tmp1)  do
	. . do:":"=$extract(output($zgbldir,region,timestamp,tmp1),$length(output($zgbldir,region,timestamp,tmp1)))
	. . . set output($zgbldir,region,timestamp,tmp1_output($zgbldir,region,timestamp,tmp1))=""
	. . . kill output($zgbldir,region,timestamp,tmp1)
	. . do:"Journal State"=$extract(tmp1,1,13)
	. . . set output($zgbldir,region,timestamp,"Journal State")=$$trimbegwhsp($extract(tmp1,14,$length(tmp1)))_" "_output($zgbldir,region,timestamp,tmp1)
	. . . kill output($zgbldir,region,timestamp,tmp1)
	quit:$quit 0 quit

errtrap	; Set error trap, if not set
	set:'$length($etrap) $etrap="set $etrap=""use $principal write $zstatus,! zhalt 1"" set tmp1=$piece($ecode,"","",2),tmp2=$text(@tmp1) if $length(tmp2) write $text(+0),@$piece(tmp2,"";"",2),! zhalt +$extract(tmp1,2,$length(tmp1))"
	quit

trimbegwhsp(s)	; Return s without leading tabs or spaces
	new i,l,tmp
	set l=$length(s) for  set tmp=$extract(s,$increment(i)) quit:" "'=tmp&($c(9)'=tmp)!'$length(tmp)
	quit $extract(s,i,$length(s))

trimendwhsp(s)	; Return s without trailing tabs or spaces
	new i,l,tmp
	set i=$length(s)+1 for  set tmp=$extract(s,$increment(i,-1)) quit:" "'=tmp&($c(9)'=tmp)!'i
	quit $extract(s,1,i)

trimwhsp(s)
	quit $$trimendwhsp($$trimbegwhsp(s))

;	Error message texts
U253	;"-F-ILLEGALDETAIL """_detail_""" is not a valid specification of details to dump"
U254	;"-F-ILLEGALSELECTION """_what_""" is not a valid selection of what to dump"
U255	;"-F-BADINVOCATION Must invoke as DO DUMP^"_$text(+0)_"(...)"
