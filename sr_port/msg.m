;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 2001, 2006 Fidelity Information Services, Inc	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
 ;
 New ansiopen,err,fn,i1,in,lo,msg,out,outansi,severe,txt,up,vms
 Set severe("warning")=0
 Set severe("success")=1
 Set severe("error")=2
 Set severe("info")=3
 Set severe("fatal")=4
 Set severe("severe")=4
 Set up="ABCDEFGHIJKLMNOPQRSTUVWXYZ"
 Set lo="abcdefghijklmnopqrstuvwxyz"
 ;
 ; On Unix, start this program with .../mumps -run msg filename osname
 ; On VMS, first compile and link msg.m into msg.exe
 ;         then    $ commandname == $disk$xxx:[dir.sub]msg.exe
 ;         then    $ commandname filename osname
 ;
 Set txt=$ZCMDLINE
 Set:$TRanslate($Piece(txt," ",1),"/RUN","-run")="-run" txt=$Piece(txt," ",3,$Length(txt)+2)
 Set fn=$Piece(txt," ",1)
 Set vms=($Select($Length(txt," ")>1:$Piece(txt," ",2),1:$Piece($ZVERSION," ",3))="VMS")
 ;
 Set in=$ZPARSE(fn,"","",".msg")
 Set out=$ZPARSE(fn,"NAME"),txt=$ZPARSE(out,"","[]",".c")
 Set l=$Length(txt),outansi=txt
 set ext=$Select(vms:".C",1:".c"),extl=ext_";"
 Set:$Extract(txt,l-1,l)=ext outansi=$Extract(txt,1,l-2)
 Set:$Extract(txt,l-2,l)=extl outansi=$Extract(txt,1,l-3)
 Set out=outansi_"_ctl.c",outansi=outansi_"_ansi.h"
 Set fn=$ZPARSE(fn,"NAME")
 Set fn=$TRanslate(fn,up,lo)
 Set cnt=0
 Open in:readonly,out:newversion
 Set ansiopen=0
 Use out Do hdr
 For  Use in Read msg Quit:$TRanslate(msg,lo,up)?.E1".FACILITY".E
 Set err=0 Do  Quit:err
 . New i1,i2,upmsg
 . ; Expect a line like:
 . ;   .FACILITY    GTM,246/PREFIX=ERR_
 . Set upmsg=$TRanslate(msg,lo_$Char(9)_",/=",up_"    ")_" "
 . For i1=1:1 Quit:$Extract(upmsg,i1)'=" "
 . If $Extract(upmsg,i1,i1+9)'=".FACILITY " Do  Set err=1 Quit
 . . Use $Principal Write !!,"Message file format error in ",in,":"
 . . Write !,$TRanslate(msg,$Char(9)," "),!?i1,"^-------^",!,"Expected: '.FACILITY'.",!
 . . Quit
 . For i1=i1+10:1 Quit:$Extract(upmsg,i1)'=" "
 . For i2=i1:1 Quit:$Extract(upmsg,i2)=" "
 . Set facility=$Extract(msg,i1,i2-1)
 . For i1=i2:1 Quit:$Extract(upmsg,i1)'=" "
 . For i2=i1:1 Quit:$Extract(upmsg,i2)=" "
 . Set facnum=$Extract(msg,i1,i2-1)
 . If facnum>2047 Do  Set err=1 Quit
 . . Use $Principal Write !!,"Message file format error in ",in,":"
 . . Write !,"Expected a number between 1 and 2047, found """,facnum,""".",!
 . . Quit
 . For i1=i2:1 Quit:$Extract(upmsg,i1)'=" "
 . For i2=i1:1 Quit:$Extract(upmsg,i2)=" "
 . If $Extract(upmsg,i1,i2)'="PREFIX " Do  Set err=1 Quit
 . . Use $Principal Write !!,"Message file format error in ",in,":"
 . . Write !,$TRanslate(msg,$Char(9)," "),!?i1,"^-----^",!,"Expected: 'PREFIX='.",!
 . . Quit
 . For i1=i2:1 Quit:$Extract(upmsg,i1)'=" "
 . For i2=i1:1 Quit:$Extract(upmsg,i2)=" "
 . Set prefix=$Extract(msg,i1,i2-1)
 . Use out Write "#include ""mdef.h""",!
 . Write "#include ""error.h""",!!
 . Write:'vms "LITDEF"_$Char(9)_"err_msg "_fn_"[] = {",!
 . Quit
 For  Use in Quit:$ZEOF  Read msg Do:$Extract(msg,1)?1u
 . New delim,i1,lomsg
 . Set cnt=cnt+1 If cnt>4095 Do
 . . Use $Principal Write !!,"Message file format error in ",in,":"
 . . Write !,"Cannot process more than 4095 messages."
 . . Write !,"Overflow occurred at:",!,msg
 . . Quit
 . ; Expect a line like:
 . ; MNEMONIC <error message text>/severity/fao=###!/ansi=### ! comment
 . ;   or:
 . ; MNEMONIC "error message text"/severity/fao=###!/ansi=### ! comment
 . For i1=1:1 Quit:$Extract($TRanslate(msg,$Char(9)," "),i1)=" "
 . Set outmsg(cnt)=$Extract(msg,1,i1-1)
 . For i1=i1:1 Quit:$Extract(msg,i1)="<"  Quit:$Extract(msg,i1)=""""
 . Set text=""""
 . Set delim=$Extract(msg,i1) For i1=i1+1:1 Do  Quit:delim=""
 . . If $Extract(msg,i1)=">",delim="<" Set delim="" Quit
 . . If $Extract(msg,i1)="""",delim="""",$Extract(msg,i1+1)'="""" Set delim="" Quit
 . . Set:$Extract(msg,i1)="""" text=text_"\" Set text=text_$Extract(msg,i1)
 . . Quit
 . Set text=text_""""
 . Set (severity,fao)="",ansi="none",lomsg=$TRanslate($Extract(msg,i1+1,$Length(msg)),up_$Char(9,32),lo)
 . For  Quit:lomsg=""  Do
 . . New key,ok,s,val
 . . If $Extract(lomsg,1,2)="!/" Set lomsg=$Extract(lomsg,2,$Length(lomsg)) Quit
 . . If $Extract(lomsg,1)="!" Set lomsg="" Quit
 . . If $Extract(lomsg,1)'="/" Do
 . . . Use $Principal Write !!,"Message file format error in ",in,":"
 . . . Write !,"All options must be preceded by a forward slash (/), Found:",!,msg
 . . . Write !,"Error encountered at: ",lomsg
 . . . Quit
 . . Set ok=0
 . . For i1=2:1:$Length(lomsg)+1 Quit:$Extract(lomsg,i1)="/"
 . . Set key=$Piece($Extract(lomsg,2,i1-1),"=",1),val=$TRanslate($Piece($Extract(lomsg,2,i1-1),"=",2),"!")
 . . If key="" Do  Quit
 . . . Use $Principal Write !!,"Message file format error in ",in,":"
 . . . Write !,"Error message specification:",!,msg
 . . . Write !,"Empty keyword encountered: ",lomsg
 . . . Quit
 . . If $Data(severe(key)) Set severity=severe(key),ok=1
 . . If 'ok,$Extract("fao",1,$Length(key))=key Set:+val=val fao=val,ok=1
 . . If 'ok,$Extract("ansi",1,$Length(key))=key Set:+val=val ansi=val,ok=1
 . . Do:'ok
 . . . Use $Principal Write !!,"Message file format error in ",in,":"
 . . . Write !,"Error message specification:",!,msg
 . . . Write !,"Option not recognized: ",lomsg
 . . . Quit
 . . Set lomsg=$Extract(lomsg,i1,$Length(lomsg))
 . . Quit
 . If severity="" Do  Quit
 . . Use $Principal Write !!,"Message file format error in ",in,":"
 . . Write !,"Error message specification:",!,msg
 . . Write !,"Severity not specified."
 . . Quit
 . If fao="" Do  Quit
 . . Use $Principal Write !!,"Message file format error in ",in,":"
 . . Write !,"Error message specification:",!,msg
 . . Write !,"Format item count (fao) not specified."
 . . Quit
 . Set outmsg(cnt,"code")=(facnum+2048)*65536+((cnt+4096)*8)+severity
 . If 'vms Use out Write $Char(9),"""",outmsg(cnt),""", ",text,", ",fao,",",!
 . If ansiopen,ansi="none" Set ansi=0 ; Make !/ansi= specification optional (except for first one)
 . Quit:ansi="none"
 . Do:'ansiopen
 . . Open outansi:newversion Use outansi
 . . Do hdr Set ansiopen=1 Write !,"const static readonly int error_ansi[] = {",!
 . . Quit
 . Use outansi Write $Char(9),$Justify(ansi,4),",",$Char(9),"/* ",outmsg(cnt)," */",!
 . Quit
 Use out
 Do:'vms
 . Write "};",!!
 . For i1=1:1:cnt Write "LITDEF",$Char(9),"int ",prefix,outmsg(i1)," = ",outmsg(i1,"code"),";",!
 . Quit
 ; VMS can have addresses in constants, most Unix platforms cannot.
 Write !,$Select(vms:"LITDEF",1:"GBLDEF"),$Char(9),"err_ctl "_fn_"_ctl = {",!
 Write $Char(9),facnum,",",!
 Write $Char(9),""""_facility_""",",!
 Write $Char(9),$Select(vms:"NULL,",1:"&"_fn_"[0],"),!
 Write $Char(9),cnt_"};",!
 If ansiopen Use outansi Write $Char(9),"};",! Close outansi
 Quit
hdr New year
 Set year=$ZDATE($Horolog,"YEAR")
 Write "/****************************************************************",!
 Write " *",$Char(9,9,9,9,9,9,9,9),"*",!
 Write " *",$Char(9),"Copyright 2001"
 Write:year'=2001 ",",year Write " Fidelity Information Services, Inc",$Char(9),"*",!
 Write " *",$Char(9,9,9,9,9,9,9,9),"*",!
 Write " *",$Char(9),"This source code contains the intellectual property",$Char(9),"*",!
 Write " *",$Char(9),"of its copyright holder(s), and is made available",$Char(9),"*",!
 Write " *",$Char(9),"under a license.  If you do not know the terms of",$Char(9),"*",!
 Write " *",$Char(9),"the license, please stop and do not read further.",$Char(9),"*",!
 Write " *",$Char(9,9,9,9,9,9,9,9),"*",!
 Write " ****************************************************************/",!!
 Quit
 ;
