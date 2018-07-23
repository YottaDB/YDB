;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
; Copyright (c) 2001-2018 Fidelity National Information		;
; Services, Inc. and/or its subsidiaries. All rights reserved.	;
;								;
; Copyright (c) 2017-2018 YottaDB LLC. and/or its subsidiaries.	;
; All rights reserved.						;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
 ;
 set $etrap="use $principal write $zstats,!! kill outmsg zshow ""*"" zhalt 1"
 New ansiopen,err,fn,i1,in,lo,msg,out,outansi,severe,txt,up,vms
 Set severe("warning")=0
 Set severe("success")=1
 Set severe("error")=2
 Set severe("info")=3
 Set severe("fatal")=4
 Set severe("severe")=4
 Set up="ABCDEFGHIJKLMNOPQRSTUVWXYZ"
 Set lo="abcdefghijklmnopqrstuvwxyz"
 set ydbplatform=$zpiece($zversion," ",3)
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
 Set undocarr=fn_"_undocarr"
 Set cnt=0,undocmsgcnt=0,lineno=0
 Open in:readonly,out:newversion
 Set ansiopen=0
 Use out Do chdr		; Create <fn>_ctl.c file and its prologue
 set ydbfn=$select("ydberrors"=fn:"ydberrors.h",1:"ydb"_fn_".h") ; Avoid naming it ydbydberrors.h
 open ydbfn:newversion		; Create ydb<fn>.h file and its prologue
 use ydbfn
 do chdr
 ;
 ; If this is merrors.msg or ydberrors.msg, we have an associated file to create:
 ;   - merrors.msg creates libydberrors.h  - header file with YDB_ERR_ #defines for each error with negated error values.
 ;   - ydberrors.msg creates libydberrors2.h - same as libydberrors.h for new YDB errors (so don't mess with merrors.msg)
 ;
 if ("merrors"=fn) do
 . set libydberrorsfn="libydberrors.h"
 . open libydberrorsfn:newversion
 . use libydberrorsfn
 . do chdr
 if ("ydberrors"=fn) do
 . set libydberrorsfn="libydberrors2.h"
 . open libydberrorsfn:newversion
 . use libydberrorsfn
 . do chdr
 ;
 For  Use in Read msg Set lineno=lineno+1 Quit:$TRanslate(msg,lo,up)?.E1".FACILITY".E
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
 ; Read all the comments until the start of "undocumented errors" section. It assumes there are only comments upto now.
 ; Read the ".TITLE" line
 Use in Read comment Set lineno=lineno+1
 Set undocmsgstart="known undocumented messages",undocmsgend="new undocumented error messages"
 For  Use in Read comment  Quit:(($ZEOF)!($Extract(comment,1)'="!")!(comment[undocmsgstart))  Set lineno=lineno+1
 If comment'[undocmsgstart  Do
 . Use $Principal Write !!,"Message file format error in ",in,":"
 . Write "Expected the section of undocumented errors starting with """_undocmsgstart_"""",!
 . Write "Line ("_lineno_") : ",comment,!
 . Quit
 ; Now the "undocumented errors" section starts. Create the array of Mnemonics
 For  Use in Read comment Set lineno=lineno+1 Quit:(($ZEOF)!($Extract(comment,1)'="!")!(comment[undocmsgend))  Do
 . Set undocmsgcnt=undocmsgcnt+1  If undocmsgcnt>4095 Do  Quit
 . . Use $Principal Write !!,"Message file format error in ",in,":"
 . . Write !,"Cannot process more than 4095 messages."
 . . Write !,"Overflow occurred at:",!,comment
 . ; Expect a line like:
 . ; !<TAB>MNEMONIC<TAB><TAB>...<error message text>
 . If $Extract(comment,1)'="!",$Extract(comment,2)'=$Char(9) Do  Quit
 . . Use $Principal Write !!,"Message file format error in ",in,":"
 . . Write !,comment,!,"^-----^",!,"Expected: '!<TAB>'.",!
 . Set i1=$Find($TRanslate(comment,$Char(9)," "),"ERR_")
 . Set i2=$Find($TRanslate(comment,$Char(9)," ")," ",i1-1)
 . If 'i2  Do  Quit
 . . Use $Principal Write !!,"Message file format error in ",in,":"
 . . Write !,comment,!,"      ^-----^",!,"Expected: a mnemonic starting with ERR_",!
 . Set undocmnemonic(undocmsgcnt)=$Extract(comment,i1,i2-2)
 For  Use in Quit:$ZEOF  Read msg Do:$Extract(msg,1)?1u
 . New delim,i1,lomsg
 . Set cnt=cnt+1 If cnt>4095 Do  Quit
 . . Use $Principal Write !!,"Message file format error in ",in,":"
 . . Write !,"Cannot process more than 4095 messages."
 . . Write !,"Overflow occurred at:",!,msg
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
 . . If $Extract(lomsg,1)'="/" Do  Quit
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
 . For msgcnt=1:1:undocmsgcnt  If outmsg(cnt)=undocmnemonic(msgcnt)  Set undocmnemonic(msgcnt,"code")=cnt-1 Quit
 . If 'vms Use out Write $Char(9),"{ """,outmsg(cnt),""", ",text,", ",fao," },",!
 . If ansiopen,ansi="none" Set ansi=0 ; Make !/ansi= specification optional (except for first one)
 . Quit:ansi="none"
 . Do:'ansiopen
 . . Open outansi:newversion Use outansi
 . . Do chdr Set ansiopen=1 Write !,"const static readonly int error_ansi[] = {",!
 . . Quit
 . Use outansi Write $Char(9),$Justify(ansi,4),",",$Char(9),"/* ",outmsg(cnt)," */",!
 . Quit
 Use out
 Write "};",!!
 Do
 . Use out
 . Write !!,"LITDEF"_$Char(9)_"int "_undocarr_"[] = {",!
 . For i1=1:1:undocmsgcnt  Write $char(9)_undocmnemonic(i1,"code")_","_$char(9)_"/* "_undocmnemonic(i1)_" */",!
 . Write "};",!!
 . Quit
 Write !,"GBLDEF",$Char(9),"err_ctl "_fn_"_ctl = {",!
 Write $Char(9),facnum,",",!
 Write $Char(9),""""_facility_""",",!
 Write $Char(9),$Select(vms:"NULL,",1:"&"_fn_"[0],"),!
 Write $Char(9),cnt_",",!
 Write $Char(9),"&"_undocarr_"[0],",!
 Write $Char(9),undocmsgcnt,!,"};",!!
 If ansiopen Use outansi Write $Char(9),"};",! Close outansi
 ;
 ; Now that we know all the error codes, write them out to the header file associated with the error file we read
 ;
 use ydbfn
 for i=1:1:cnt write "#define ",prefix,outmsg(i)," ",outmsg(i,"code"),!
 write:("merrors"=fn) !,"#include ""ydberrors.h""",!	; Daisy chain this to the file created for ydberrors.msg
 close ydbfn
 ;
 ; Write out additional header files if needed
 ;
 do:(("merrors"=fn)!("ydberrors"=fn))
 . ;
 . ; Write the negative values of errors used by libyottadb and its users
 . ;
 . use libydberrorsfn
 . for i=1:1:cnt write "#define YDB_ERR_",outmsg(i)," -",outmsg(i,"code"),!
 . write:("merrors"=fn) !,"#include ""libydberrors2.h""",!	; Daisy chain this to the file created for ydberrors.msg
 . close libydberrorsfn
 Quit

;
; Routine to write C header file for the generated_ctl.c file and the C header files
;
chdr
 Set saveIO=$IO
 If vms Set cfile=$ztrnlnm("gtm$src")_"copyright.txt"
 If 'vms Set cfile=$ztrnlnm("gtm_tools")_"/copyright.txt"
 Set xxxx="2001"
 Set yyyy=$zdate($H,"YYYY")
 Open cfile:read
 Use saveIO w "/****************************************************************",!
 For i=1:1 Use cfile Read line Quit:$zeof  Do
 . If (1<$zlength(line,"XXXX")) Do
 . . Set str=$zpiece(line,"XXXX",1)_xxxx_$zpiece(line,"XXXX",2)
 . . Set str=$zpiece(str,"YYYY",1)_yyyy_$zpiece(str,"YYYY",2)
 . Else  Do
 . . if (1<$zlength(line,"YYYY")) Do
 . . . Set str=$zpiece(line,"YYYY",1)_yyyy_$zpiece(line,"YYYY",2)
 . . Else  Do
 . . . Set str=line
 . Use saveIO Write " *"_str_"*",!
 Close cfile
 Use saveIO
 Write " ****************************************************************/",!!
 Quit
