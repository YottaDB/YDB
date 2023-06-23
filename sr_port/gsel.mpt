;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
; Copyright (c) 1987-2018 Fidelity National Information		;
; Services, Inc. and/or its subsidiaries. All rights reserved.	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%GSEL	;GT.M %GSEL utility - global select into a local array
	;invoke ^%GSEL to create %ZG - a local array of existing globals, interactively
	;
	NEW add,beg,cnt,d,end,g,gd,gdf,k,out,pat,stp,nfe
	DO init,main
	USE:$DATA(d("use")) @d("use")
	USE:$DATA(d("io")) d("io")
	QUIT

GD	;
	NEW add,beg,cnt,d,end,g,gd,gdf,k,out,pat,stp,nfe
	SET cnt=0,(out,gd,gdf)=1
	DO main
	IF gdf SET %ZG="*" DO setup,it WRITE !,"Total of ",cnt," global",$SELECT(cnt=1:".",1:"s."),!
	USE:$DATA(d("use")) @d("use")
	USE:$DATA(d("io")) d("io")
	QUIT

CALL	;invoke %GSEL without clearing %ZG (%ZG stores the list of globals from %GSEL searches)
	NEW add,beg,cnt,d,end,g,gd,gdf,k,out,pat,stp,nfe
	SET (cnt,gd)=0
	IF $DATA(%ZG)>1 SET g="" FOR  SET g=$ORDER(%ZG(g)) QUIT:'$LENGTH(g)  SET cnt=cnt+1
	IF $GET(%ZG)'?.N SET out=0 DO setup,it SET %ZG=cnt QUIT
	SET out=1
	DO main
	USE:$DATA(d("use")) @d("use")
	USE:$DATA(d("io")) d("io")
	QUIT

init	;
	KILL %ZG
	SET (cnt,gd)=0,out=1
	QUIT

main	;
	SET d("io")=$IO
	if '$DATA(%zdebug) NEW $ETRAP SET $ETRAP="ZGOTO "_$ZLEVEL_":ERR^"_$TEXT(+0) DO
	. NEW x
	. ZSHOW "d":d									; save original $p settings
	. SET x=$PIECE($PIECE(d("D",1),"CTRA=",2)," ")
	. SET:""=x x=""""""
	. SET d("use")="$PRINCIPAL:(CTRAP="_x_":EXCEPTION=",x=$PIECE(d("D",1),"EXCE=",2),x=$ZWRITE($EXTRACT(x,2,$LENGTH(x)-1))
	. SET:""=x x=""""""
	. SET d("use")=d("use")_x_":"_$SELECT($FIND(d("D",1),"NOCENE"):"NOCENABLE",1:"CENABLE")_")"
	. USE $PRINCIPAL:(CTRAP=$CHAR(3,4):EXCEPTION="":NOCENABLE)
	FOR  DO inter QUIT:'$LENGTH(%ZG)
	SET %ZG=cnt
	QUIT

inter	;
	SET nfe=0
	READ !,"Global ^",%ZG,! QUIT:'$LENGTH(%ZG)
	IF $EXTRACT(%ZG)="?",$LENGTH(%ZG)=1 DO help QUIT
	IF (%ZG="?D")!(%ZG="?d") DO cur QUIT
	IF $EXTRACT(%ZG)="?" SET nfe=1 DO nonfatal QUIT
	DO setup IF nfe>0 SET nfe=0,gdf=0 QUIT
	DO it
	WRITE !,$SELECT(gd:"T",1:"Current t"),"otal of ",cnt," global",$SELECT(cnt=1:".",1:"s."),!
	QUIT

setup	;Handles the base case of no range
	NEW g1,et
	IF gd SET add=1,cnt=0,g=%ZG KILL %ZG SET %ZG=g
	ELSE  IF "'-"[$EXTRACT(%ZG) SET add=0,g=$EXTRACT(%ZG,2,999)
	ELSE  SET add=1,g=%ZG
	SET g1=$TRANSLATE(g,"?%*","aaa") ;Substitute wildcards for valid characters
	DO
	. SET et=$ETRAP NEW $ETRAP,$ESTACK SET $ETRAP="SET $ECODE="""",$ETRAP=et DO setup2",g1=$QSUBSCRIPT(g1,1)
	. SET:$FIND(g,"(")'=0 $EXTRACT(g,$FIND(g,"(")-1,$LENGTH(g))=""
	. SET:$EXTRACT(g)="^" $EXTRACT(g)=""
	IF "?"=$EXTRACT(g,$FIND(g,":")) SET nfe=1 DO nonfatal QUIT
	SET g=$TRANSLATE(g,"?","%"),beg=$PIECE(g,":",1),end=$PIECE(g,":",2)
	IF end=beg SET end=""
	QUIT

setup2	;Handles the case of a range argument
	NEW p,q,x,di,beg1
	SET p=$LENGTH(g1,":")
	IF p<2 SET nfe=2 DO nonfatal QUIT
	ELSE  IF p>2 SET q=$LENGTH(g1,"""")-1 FOR x=2:2:q SET $PIECE(g1,"""",x)=$TRANSLATE($PIECE(g1,"""",x),":","a")
	DO
	. SET beg=$PIECE(g1,":",1),end=$PIECE(g1,":",2)
        . NEW $ETRAP,$ESTACK SET $ETRAP="SET nfe=2 DO nonfatal",beg1=$QSUBSCRIPT(beg,0),end=$QSUBSCRIPT(end,0)
	QUIT:nfe>0
	SET di=$LENGTH(beg),beg=$EXTRACT(g,1,di),end=$EXTRACT(g,di+2,$LENGTH(g))
	SET:$FIND(beg,"(")'=0 $EXTRACT(beg,$FIND(beg,"(")-1,$LENGTH(beg))=""
	SET:$EXTRACT(beg)="^" $EXTRACT(beg)=""
	SET:$FIND(end,"(")'=0 $EXTRACT(end,$FIND(end,"(")-1,$LENGTH(end))=""
	SET:$EXTRACT(end)="^" $EXTRACT(end)=""
	SET g=beg_":"_end
	QUIT

it	;
	SET gdf=0
	IF end'?."*",end']beg QUIT
	SET g=beg DO pat
	IF pat["""" DO start FOR  DO search QUIT:'$LENGTH(g)  DO save
	IF pat["""",'$LENGTH(end) QUIT
	SET beg=stp
	SET:'$LENGTH(g) g=stp
	SET pat=".E",stp="^"_$EXTRACT(end)_$TRANSLATE($EXTRACT(end,2,9999),"%","z")
	DO start FOR  DO search QUIT:'$LENGTH(g)  DO save
	SET g=end DO pat
	IF pat["""" SET:beg]g g=beg DO start FOR  DO search QUIT:'$LENGTH(g)  DO save
	QUIT

pat	;
	NEW tmpstp
	SET:"%"=$EXTRACT(g) g="!"_$EXTRACT(g,2,9999)
	SET pat=g
	FOR  QUIT:$LENGTH(g,"%")<2  DO
	.SET g=$PIECE(g,"%",1)_"#"_$PIECE(g,"%",2,999),pat=$PIECE(pat,"%",1)_"""1E1"""_$PIECE(pat,"%",2,999)
	FOR  QUIT:$LENGTH(g,"*")<2  DO
	.SET g=$PIECE(g,"*",1)_"$"_$PIECE(g,"*",2,999),pat=$PIECE(pat,"*",1)_""".E1"""_$PIECE(pat,"*",2,999)
	SET:"!"=$EXTRACT(g) g="%"_$EXTRACT(g,2,9999),pat="%"_$EXTRACT(pat,2,9999)
	IF pat["""" SET pat="1""^"_pat_""""
	SET tmpstp="z",$PIECE(tmpstp,"z",30)="z"
	SET g="^"_$PIECE($PIECE(g,"#"),"$"),stp=g_$EXTRACT(tmpstp,$LENGTH(g)-1,31)
	QUIT

start	;
	SET:"^"=g g="^%"
	IF g?@pat,$DATA(@g) DO save
	QUIT

search	;
	FOR  SET g=$ORDER(@g) SET:g]stp g="" QUIT:g?@pat!'$LENGTH(g)
	QUIT

save	;
	IF add,'$DATA(%ZG(g)) SET %ZG(g)="",cnt=cnt+1 DO prt:out
	IF 'add,$DATA(%ZG(g)) KILL %ZG(g) SET cnt=cnt-1 DO prt:out
	QUIT

prt	;
	WRITE:$X>70 ! WRITE g,?$X\10+1*10
	QUIT

help	;
	WRITE !,?2,"<RET>",?25,"to leave",!,?2,"""*""",?25,"for all"
	WRITE !,?2,"global",?25,"for 1 global"
	WRITE !,?2,"global1:global2",?25,"for a range"
	WRITE !,?2,"""*"" as a wildcard",?25,"permitting any number of characters"
	WRITE !,?2,"""%"" as a wildcard",?25,"for a single character in positions other than the first"
	WRITE !,?2,"""?"" as a wildcard",?25,"for a single character in positions other than the first"
	QUIT:gd
	WRITE !,?2,"""'"" as the 1st character",!,?25,"to remove globals from the list"
	WRITE !,?2,"?D",?25,"for the currently selected globals",!
	QUIT

cur	;
	SET g=""
	FOR  SET g=$ORDER(%ZG(g)) QUIT:'$LENGTH(g)  WRITE:$X>70 ! WRITE g,?($X\10+1*10)
	WRITE !,$SELECT(gd:"T",1:"Current t"),"otal of ",cnt," global",$SELECT(cnt=1:".",1:"s."),!
	QUIT

nonfatal
	NEW ecde
	SET $ECODE=""
	IF nfe=1 SET ecde="U257"
	ELSE  IF nfe=2 SET ecde="U258"
	WRITE $TEXT(+0),@$PIECE($TEXT(@ecde),";",2),!
	QUIT

ERR	;
	USE:$DATA(d("use")) @d("use")
	USE:$DATA(d("io")) d("io")
	SET $ECODE=""
	QUIT

LOOP
	DO main
	USE $PRINCIPAL:(ctrap=$CHAR(3,4):exc="")
	QUIT

;	Error message texts
U257	;"-E-ILLEGALUSE Illegal use of ""?"". Only valid as 1st character when ""?D"" or ""?d"""
U258	;"-E-INVALIDGBL Search string either uses invalid characters or is improperly formated"
