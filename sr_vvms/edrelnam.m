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
edrelnam	; ; ; verify a proposed release name and update release_name.h
	;
	set $zro="[]/src=([],gtm$vrt:[pct])"
	Write "$ztrnlnm(""gtm_new_ver_id"") = """,$ztrnlnm("gtm_new_ver_id"),"""",!
	set relnam=$$FUNC^%UCASE($ztrnlnm("gtm_new_ver_id")),suffix=$piece(relnam,"-",2)
	if $p(relnam,"-")'?1"V"1.2N1"."1.2N&'((suffix?1.2N.1A)!(suffix?1"BL"1.2N)!(suffix?1"FT"1.2N)) do
	.	write !,"The parameter """,relnam,""" (gtm_new_ver_id) is not a valid format",!
	.	set $ztrap="" zmessage 2
	set file="release_name.h",new="temp.tmp"
	open file:(read:exception="goto noopen"),new:newversion
	;
	Use file:exception="goto eof"
	For  Read line  Do
	.	Use new
	.	If line["GTM_RELEASE_NAME" Do
	.	.	Write $Piece(line,"V"),relnam," ",$Piece($Piece(line,"V",2,999)," ",2,99),!
	.	Else  Do
	.	.	If line["GTM_VERSION" Write $Piece(line,"V",1,2),$Piece(relnam,"-"),"""",!
	.	.	Else                  Write line,!
	.	Use file:exception="goto eof"
	Quit
	;
eof	close file,new:rename=file
	quit
	;
noopen	write !,"Unable to open release_name.h",!
	set $ztrap="" zmessage 2
