;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
spkitbld	; ; ; edit the gtm$vrt:[t%%]*_spkitbld.dat version
	;
	New
	Set cnt=0
	New $ZTrap Set $ZTrap="Use $Principal Write !,""FAILED!!"",!,$ZStatus ZGoto "_$ZLevel_":error"
	Set gtmvrt=$TRanslate($ZCMDLINE,""""),temp=gtmvrt_"temp.dat",mask=$Piece(gtmvrt,"]")_".t%%]*_spkitbld.dat"
	If gtmvrt=""!(gtmvrt="/DIR") Do		;arg overrides gtm$vrt
	. Set gtmvrt=$ZTRNLNM("gtm$vrt"),temp="gtm$vrt:[000000]temp.dat",mask="gtm$vrt:[t%%]*_spkitbld.dat"
	Set gtmvrt=$Piece(gtmvrt,"]")
	Set newver=$$FUNC^%ucase($Piece(gtmvrt,".",2))
	If newver'?1"V"2N.2A1.3N.1A Write !,"Invalid version designation" Quit
	Set newver=$Extract(newver,2,9999)
	For tv="BL","FT" If newver[tv Set newver=$Piece(newver,tv)_$Select(tv="BL":88,1:99)_$Piece(newver,tv,2)
	If newver?.N1.A For i=$Length(newver):-1:2 Set newver=$Extract(newver,1,i-1)_($Ascii(newver,i)#10)_$Extract(newver,i+1,9999)
	Set file=$ZSEARCH("foo.bar")	;clear any current search
	For  Set file=$Piece($ZSEARCH(mask),";") Quit:'$l(file)  Do
	. Open file:(readonly:exception="Goto eof"),temp:newversion
	. Use file
	. Read line
	. Set oldver=$Piece(line," ",3)
	. For i=1:1:$l(oldver) Quit:$Extract(oldver,i)?1N
	. Set prod=$Extract(oldver,1,i-1)
	. Set $Piece(line," ",3)=prod_newver
	. Use temp
	. Write line,!
	. For  Use file Read line Use temp Write line,!
eof	. Close file:delete
	. Close temp:rename=file
	. Set cnt=cnt+1
	Write !,"Complete"
error	Write !,"Updated ",cnt," files",!
	Quit
