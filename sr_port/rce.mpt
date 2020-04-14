;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
; Copyright (c) 1988-2019 Fidelity National Information		;
; Services, Inc. and/or its subsidiaries. All rights reserved.	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%RCE	;GT.M %RCE utility - change every occurrence of a string in one or more routines
	;invoke ^%RCE to get interaction
	;invoke CALL^%RCE with %ZF - string to find, %ZN - new string, %ZR - routine array or name,
	;			%ZD an optional device to receive a trail
	;
	new cnt1,cnt2,cnt3,d,fnd,h,i,o,out,outd,r,tf,x,xn,%ZC,%ZD,%ZF,%ZN,%ZR
	if '$data(%ZQ) new %ZQ set %ZQ=0
	set $zstatus=""
	if '$data(%zdebug) new $etrap set $etrap="zgoto "_$zlevel_":ERR^"_$text(+0) do
	. zshow "d":d										; save original $p settings
	. set x=$piece($piece(d("D",1),"CTRA=",2)," ")
	. set:""=x x=""""""
	. set d("use")="$principal:(ctrap="_x
	. set x=$piece(d("D",1),"EXCE=",2),x=$zwrite($extract(x,2,$length(x)-1))
	. set:""=x x=""""""
	. set d("use")=d("use")_":exception="_x_":"_$select($find(d("D",1),"NOCENE"):"nocenable",1:"cenable")_")"
	. use $principal:(ctrap=$char(3,4):exception="halt:$zeof!($zstatus[""TERMWRITE"")  "_$etrap:nocenable)
	do init,MAIN
	use:$data(d("use")) @d("use")
	use:$data(d("io")) d("io")
	quit
CALL	quit:'$length($get(%ZF))
	new zc,zd
	set zc=$get(%ZC),zd=$get(%ZD)
	new cnt1,cnt2,cnt3,fnd,h,i,o,out,outd,r,tf,x,xn,%ZD,%ZC,lzd
	new:'$data(%ZN) %ZN
	if '$data(%ZQ) new %ZQ set %ZQ=0
	set %ZC=zc,%ZD=zd
	set %ZD=$get(%ZD),%ZN=$get(%ZN),(cnt1,cnt2,cnt3,out)=0,tf=$job_"rce.tmp",lzd=$length(%ZD),outd=%ZQ
	set:'lzd %ZD=$principal
	set:%ZC outd=$length(%ZD)
	set:'outd %ZD=$principal
	set:'lzd %ZD=$principal
	set %ZC=1
	do CALL^%RSEL:10>$data(%ZR),work
	quit
init	set %ZC=1,(cnt1,cnt2,cnt3)=0,out=1,tf=$job_"rce.tmp"
	write !,"Routine Change Every occurrence",!
	quit
MAIN	set %ZR=""
	do CALL^%RSEL
	if '%ZR write !,"No routines selected" quit
	write !,$select(%ZC:"Old",1:"Find")," string: "
	read %ZF
	if '$length(%ZF) write !,"No search string to find - no search performed",! quit
	write:%ZF?.E1C.E !,"The find string contains control characters"
	read:%ZC !,"New string: ",%ZN
	if %ZC,%ZN?.E1C.E write !,"The New string contains control characters"
	write !,$select(%ZC:"Replace",1:"Find")," all occurrences of:",!,">",%ZF,"<",!
	write:%ZC "With: ",!,">",%ZN,"<",!
	if %ZC for  read !,"Show changed lines <Yes>?: ",x,! quit:"?"'=$extract(x)  do help
	if %ZC,$length(x) quit:"\QUIT"[("\"_$translate(x,"quit","QUIT"))  do
	. set outd=$extract("NO",1,$length(x))'=$extract($translate(x,"no","NO"),1,2)
	else  set outd=1
	if outd for  do  quit:$length(%ZD)
	. read !,"Output device: <terminal>: ",%ZD,!
	. if '$length(%ZD) set %ZD=$principal quit
	. quit:"^"=%ZD
	. if "?"=%ZD do  quit
	 . . write !!,"Select the device you want for output"
	 . . write !,"If you wish to exit enter a carat (^)",!
	 . . set %ZD=""
	. if ""=$zparse(%ZD) write "  no such device" set %ZD="" quit
	. open %ZD:(newversion:block=2048:record=2044:exception="goto noopen"):0
	. else  write !,%ZD," is not available" set %ZD=""
	. quit
noopen	. write !,$piece($ZS,",",2,999),!
	. close %ZD
	. set %ZD=""
	. quit
	set:'$data(%ZD) %ZD=""
	quit:"^"=%ZD
	write !
	do work
	quit
work	set %ZR="",r=$zsearch("__")
	if outd,$principal'=%ZD do
	. use %ZD
	. write $zdate($h,"DD-MON-YEAR 24:60:SS"),!
	. write "Routine ",$select(%ZC:"Change",1:"Search for")," Every occurrence of:",!,">",%ZF,"<",!
	. write:%ZC "To:",!,">",%ZN,"<",!
	do:'%ZC
	. set gtmvt=$$GTMVT^%GSE
	. if gtmvt set sx=$char(27)_"[7m"_%ZF_$char(27)_"[0m"
	. else  set sx=%ZF,flen=$length(%ZF),tics=$translate($justify("",flen)," ","^")
	for  set %ZR=$order(%ZR(%ZR)) quit:'$length(%ZR)  do scan
	quit:'out
	use %ZD
	write !!,"Total of ",cnt1," routine",$select(cnt1=1:"",1:"s")," parsed.",!,cnt2," occurrence"
	write $select(cnt2=1:" ",1:"s "),$select(%ZC:"changed",1:"found")," in ",cnt3," routine",$select(cnt3=1:".",1:"s."),!
	close %ZD
	quit
scan(r)	new wrotezr
	set r=%ZR(%ZR)_$translate($extract(%ZR),"%","_")_$extract(%ZR,2,9999)_".m"
	set o=$zsearch(r),fnd=0,wrotezr=0
	use $principal
	if out,$principal'=%ZD!'outd write:$x>70 ! write %ZR,?$x\10+1*10
	if %ZC,tf'["/" set tf=$zparse(r,"DIRECTORY")_tf
	open:%ZC tf:(newversion:exception="set fnd=0 goto reof")
	open o:(readonly:record=2048:rewind:exception="goto rnoopen")
	use o:exception="goto reof"
	set cnt1=cnt1+1
	for  use o read x set h=$length(x,%ZF) do
	. if 'wrotezr,outd,('%ZQ!(1'=h)) use %ZD write !!,r set wrotezr=1
	. if 1=h do:%ZC  quit
	. . use tf write x,!
	. set fnd=fnd+h-1
	. if %ZC do  quit
	. . if outd use %ZD write !,"Was: ",x
	. . set xn=""
	. . for i=1:1:h-1 set xn=xn_$piece(x,%ZF,i)_%ZN
	. . set xn=xn_$piece(x,%ZF,h)
	. . write:outd !,"Now: ",xn
	. . use tf
	. . write xn,!
	. use %ZD
	. write !
	. set rl=""
	. for i=1:1:h-1 set p=$translate($piece(x,%ZF,i),$char(9)," ") write p,sx set:'gtmvt rl=rl_$justify(tics,$length(p)+flen)
	. write $piece(x,%ZF,h)
	. write:'gtmvt !,rl
	quit
reof	if fnd set cnt2=cnt2+fnd,cnt3=cnt3+1 if %ZC close o:(DELETE),tf:(RENAME=r)
	else  close o close:%ZC tf:(DELETE)
	; warning - fall-through
rnoopen	write:$zstatus'["EOF" !,$piece($zs,",",2,999),!,o
	quit
help	if "Dd"[$extract(x,2),$length(x)=2 do cur quit
	write:%ZC !,"Answer No to this prompt if you do not wish a trail of the changes"
	write !,"Enter Q to exit",!
	write !,"?D for the current routine selection"
	quit
cur	write !
	set (d,r)=""
	for  set r=$order(%ZR(r)) quit:'$length(r)  write:$x>70 ! write r,?$x\10+1*10
	quit
ERR	close:$data(tf) tf:(DELETE)
	close:$data(o) o
	if $io=$principal,(($zstatus["IOEOF")!($zstatus["TERMWRITE")) halt
	if $data(%ZD),$principal'=%ZD close %ZD
	write !,$piece($ZS,",",2,999),!
	use:$data(d("use")) @d("use")
	use:$data(d("io")) d("io")
	set $ecode=""
	quit
LOOP	close:$data(tf) tf:delete
	close:$data(o) o
	if $data(%ZD),$principal'=%ZD close %ZD
	do MAIN
	use:$data(d("use")) @d("use")
	use:$data(d("io")) d("io")
	quit
QUIET	new %ZQ
	set %ZQ=1
	do %RCE
	quit
QCALL	new %ZQ
	set %ZQ=1
	do CALL
	quit
