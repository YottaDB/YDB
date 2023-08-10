;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
; Copyright (c) 2001-2021 Fidelity National Information		;
; Services, Inc. and/or its subsidiaries. All rights reserved.	;
;								;
; Copyright (c) 2017-2023 YottaDB LLC and/or its subsidiaries.	;
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
 New ansiopen,err,fn,i1,in,lo,msg,out,outansi,severe,txt,up
 Set (severe("warning"),ival(0))=0
 Set (severe("success"),ival("A"))=1
 Set (severe("error"),ival("B"))=2
 Set (severe("info"),ival("D"))=3
 Set (severe("fatal"),severe("severe"),ival("I"),ival("S"))=4	; treating Spanning errors as Index makes them report as Data
 Set ival("T")=5
 Set up="ABCDEFGHIJKLMNOPQRSTUVWXYZ"
 Set lo="abcdefghijklmnopqrstuvwxyz"
 ; On the slowest server, this routine took less than half a second. So, there is no need to wait for more than 30 seconds.
 Set $ztimeout="30:use $principal write ""# msg.m took longer than expected"",! zshow ""*"" zhalt 1"
 ;
 ; Run this program as $gtm_exe/mumps -run msg filename
 ; If used in a non-build environment, pass "nohdr" as second argument to not depend on the presence of $gtm_tools/copyright.txt
 ; (nohdr skips copyright insertion).
 ;
 Set txt=$ZCMDLINE
 Set fn=$Piece(txt," ",1)
 Set nohdr=$Piece(txt," ",2)
 ;
 If $ztrnlnm("gtm_tools")="" write "ERROR: $gtm_tools must be defined!",! zhalt 1
 Set in=$ZPARSE(fn,"","",".msg")
 Set out=$ZPARSE(fn,"NAME"),txt=$ZPARSE(out,"","[]",".c")
 Set l=$Length(txt),outansi=txt
 set ext=".c",extl=ext_";"
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
 . . Zhalt 1
 . For i1=i1+10:1 Quit:$Extract(upmsg,i1)'=" "
 . For i2=i1:1 Quit:$Extract(upmsg,i2)=" "
 . Set facility=$Extract(msg,i1,i2-1)
 . For i1=i2:1 Quit:$Extract(upmsg,i1)'=" "
 . For i2=i1:1 Quit:$Extract(upmsg,i2)=" "
 . Set facnum=$Extract(msg,i1,i2-1)
 . If facnum>2047 Do  Set err=1 Quit
 . . Use $Principal Write !!,"Message file format error in ",in,":"
 . . Write !,"Expected a number between 1 and 2047, found """,facnum,""".",!
 . . Zhalt 1
 . For i1=i2:1 Quit:$Extract(upmsg,i1)'=" "
 . For i2=i1:1 Quit:$Extract(upmsg,i2)=" "
 . If $Extract(upmsg,i1,i2)'="PREFIX " Do  Set err=1 Quit
 . . Use $Principal Write !!,"Message file format error in ",in,":"
 . . Write !,$TRanslate(msg,$Char(9)," "),!?i1,"^-----^",!,"Expected: 'PREFIX='.",!
 . . Zhalt 1
 . For i1=i2:1 Quit:$Extract(upmsg,i1)'=" "
 . For i2=i1:1 Quit:$Extract(upmsg,i2)=" "
 . Set prefix=$Extract(msg,i1,i2-1)
 . Use out Write "#include ""mdef.h""",!
 . Write "#include ""error.h""",!!
 . Write "LITDEF"_$Char(9)_"err_msg "_fn_"[] = {",!
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
 . Zhalt 1
 ; Now the "undocumented errors" section starts. Create the array of Mnemonics
 For  Use in Read comment Set lineno=lineno+1 Quit:(($ZEOF)!($Extract(comment,1)'="!")!(comment[undocmsgend))  Do
 . Set undocmsgcnt=undocmsgcnt+1  If undocmsgcnt>4095 Do  Quit
 . . Use $Principal Write !!,"Message file format error in ",in,":"
 . . Write !,"Cannot process more than 4095 messages."
 . . Write !,"Overflow occurred at:",!,comment
 . . Zhalt 1
 . ; Expect a line like:
 . ; !<TAB>MNEMONIC<TAB><TAB>...<error message text>
 . If $Extract(comment,1)'="!",$Extract(comment,2)'=$Char(9) Do  Quit
 . . Use $Principal Write !!,"Message file format error in ",in,":"
 . . Write !,comment,!,"^-----^",!,"Expected: '!<TAB>'.",!
 . . Zhalt 1
 . Set i1=$Find($TRanslate(comment,$Char(9)," "),"ERR_")
 . Set i2=$Find($TRanslate(comment,$Char(9)," ")," ",i1-1)
 . If 'i2  Do  Quit
 . . Use $Principal Write !!,"Message file format error in ",in,":"
 . . Write !,comment,!,"      ^-----^",!,"Expected: a mnemonic starting with ERR_",!
 . . Zhalt 1
 . Set undocmnemonic(undocmsgcnt)=$Extract(comment,i1,i2-2)
 For  Use in Quit:$ZEOF  Read msg Do:$Extract(msg,1)?1u
 . new delim,i1,lomsg,flag,mtail,mhead,msgsav
 . Set cnt=cnt+1 If cnt>4095 Do  Quit
 . . Use $Principal Write !!,"Message file format error in ",in,":"
 . . Write !,"Cannot process more than 4095 messages."
 . . Write !,"Overflow occurred at:",!,msg
 . . Zhalt 1
 . ; Expect a line like:
 . ; MNEMONIC <error message text>/severity/fao=###!/ansi=###/integ-id/flag ! comment
 . ;   or:
 . ; MNEMONIC "error message text"/severity/fao=###!/ansi=###/integ-id/flag ! comment - integ-id is currently optional
 . ; as is flag, but if flag is present integ-id must also be, at least as an empty (//) field.
 . ;
  . For i1=1:1 Quit:" "=$Extract($TRanslate(msg,$Char(9)," "),i1)
    . ; We want to start by parsing our msg from the right, so we can strip off the new flag field
    . ; before moving on to the code which does not understand that.
    . ; There are four cases for integ & flag:
    . ;   0 no integ / no flag   ie: "ansi=0$"
    . ;   1 no integ / flag      ie: "ansi=0//1$"
    . ;   2 integ    / no flag   ie: "ansi=0/A$"
    . ;   3 integ    / flag      ie: "ansi=0/A/1$"
    . ; Cases 0 & 2 are handled by falling into the pre-flag code, so we only care about cases 1 & 3 where
    . ; we extract the flag and patch the message string to pull off the info the pre-flag code doesn't parse.
    . set flag=0 ; default
    . set msgsav=msg
    . if $length(msg,"//")=2 set flag=$piece(msg,"//",2),msg=$piece(msg,"//",1) ; Case 1
    . do:$length($piece(msg,"ansi=",2),"/")=3 ; Case 3
    . . set mhead=$piece(msg,"ansi=",1),mtail=$piece(msg,"ansi=",2),flag=$piece(mtail,"/",3)
    . . set msg=mhead_"ansi="_$piece(mtail,"/",1)_"/"_$piece(mtail,"/",2)
    . . quit
 . Do:(7<flag)!(0>flag)
 . . Use $Principal Write !!,"Message file format error in ",in,":"
 . . Write !,"Error message specification:",!,msgsav
 . . Write !,"Bad flag ("_flag_") encountered: ",msgsav,!
 . . Zhalt 1
 . Set outmsg(cnt)=$Extract(msg,1,i1-1)
 . ; Extract the contents between "<" and ">" and store it in the local "text"
 . For i1=i1:1:$Length(msg) Quit:$Extract(msg,i1)="<"  Quit:$Extract(msg,i1)=""""
 . Do:(i1=$Length(msg))
 . . Use $Principal Write !!,"Message file format error in ",in,":"
 . . Write !,msg,!,"Did not find opening delimiter : <",!
 . . Zhalt 1
 . Set text=""""
 . Set delim=$Extract(msg,i1) For i1=i1+1:1:$Length(msg) Do  Quit:delim=""
 . . If $Extract(msg,i1)=">",delim="<" Set delim="" Quit
 . . If $Extract(msg,i1)="""",delim="""",$Extract(msg,i1+1)'="""" Set delim="" Quit
 . . Set:$Extract(msg,i1)="""" text=text_"\" Set text=text_$Extract(msg,i1)
 . . Quit
 . Do:(i1=$Length(msg))
 . . Use $Principal Write !!,"Message file format error in ",in,":"
 . . Write !,msg,!,"Did not find closing delimiter : >",!
 . . Zhalt 1
 . Set text=text_""""
 . ; Now parse the conent left after extracting <...>
 . Set (severity,fao,ival)="",ansi="none",lomsg=$TRanslate($Extract(msg,i1+1,$Length(msg)),up_$Char(9,32),lo)
 . For  Quit:lomsg=""  Do
 . . New key,ok,s,val
 . . If $Extract(lomsg,1,2)="!/" Set lomsg=$Extract(lomsg,2,$Length(lomsg)) Quit
 . . If $Extract(lomsg,1)="!" Set lomsg="" Quit
 . . If $Extract(lomsg,1)'="/" Do  Quit
 . . . Use $Principal Write !!,"Message file format error in ",in,":"
 . . . Write !,"All options must be preceded by a forward slash (/), Found:",!,msg
 . . . Write !,"Error encountered at: ",lomsg
 . . . Zhalt 1
 . . Set ok=0
 . . For i1=2:1:$Length(lomsg)+1 Quit:$Extract(lomsg,i1)="/"
 . . If ""=ival Do  Quit
 . . . Set ival=$ZCONVERT($Piece($Piece(lomsg,"ansi=",2),"/",2),"U")
 . . . If ""=ival Set ival=0 Quit
 . . . Set lomsg=$Extract(lomsg,1,$Length(lomsg)-$Length(ival)-$Length("/"))
 . . Set key=$Piece($Extract(lomsg,2,i1-1),"=",1),val=$TRanslate($Piece($Extract(lomsg,2,i1-1),"=",2),"!")
 . . If key="" Do  Quit
 . . . Use $Principal Write !!,"Message file format error in ",in,":"
 . . . Write !,"Error message specification:",!,msg
 . . . Write !,"Empty keyword encountered: ",lomsg
 . . . Zhalt 1
 . . If $Data(severe(key)) Set severity=severe(key),ok=1
 . . If 'ok,$Extract("fao",1,$Length(key))=key Set:+val=val fao=val,ok=1
 . . If 'ok,$Extract("ansi",1,$Length(key))=key Set:+val=val ansi=val,ok=1
 . . if ok,""'=ival,'$Data(ival(ival)) Set ok=0,lomsg=ival
 . . Do:'ok
 . . . Use $Principal Write !!,"Message file format error in ",in,":"
 . . . Write !,"Error message specification:",!,msg
 . . . Write !,"Option not recognized: ",lomsg
 . . . Zhalt 1
 . . Set lomsg=$Extract(lomsg,i1,$Length(lomsg))
 . . Quit
 . If severity="" Do  Quit
 . . Use $Principal Write !!,"Message file format error in ",in,":"
 . . Write !,"Error message specification:",!,msg
 . . Write !,"Severity not specified."
 . . Zhalt 1
 . If fao="" Do  Quit
 . . Use $Principal Write !!,"Message file format error in ",in,":"
 . . Write !,"Error message specification:",!,msg
 . . Write !,"Format item count (fao) not specified."
 . . Zhalt 1
 . Set outmsg(cnt,"code")=(flag*268435456)+((facnum+2048)*65536)+((cnt+4096)*8)+severity
 . For msgcnt=1:1:undocmsgcnt  If outmsg(cnt)=undocmnemonic(msgcnt)  Set undocmnemonic(msgcnt,"code")=cnt-1 Quit
 . Use out Write $Char(9),"{ """,outmsg(cnt),""", ",text,", ",fao,", ",ival(ival)," },",!
 . If ansiopen,ansi="none" Set ansi=0 ; Make !/ansi= specification optional (except for first one)
 . Quit:ansi="none"
 . Do:'ansiopen
 . . Open outansi:newversion Use outansi
 . . Do chdr Set ansiopen=1 Write !,"const static readonly int error_ansi[] = {",!
 . . Quit
 . Set:""'=ival integ(outmsg(cnt))=ival
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
 Write $Char(9),"&"_fn_"[0],",!
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
 ; 3 following lines produce a sorted list & count of mupip integ errors that have been categorized for the table in the A&O guide
 ;zwrite integ
 ;set x="",ival=0 for  set x=$o(integ(x)) quit:'$length(x)  if $increment(ival)
 ;zwrite ival
 Quit

;
; Routine to write C header file for the generated_ctl.c file and the C header files
;
chdr
 Quit:"nohdr"=nohdr
 Set saveIO=$IO
 Set cfile=$ztrnlnm("gtm_tools")_"/copyright.txt"
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
