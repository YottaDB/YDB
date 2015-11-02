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
%MPIECE(str,delim,newdelim)
	; Replace one or more consecutive occurrences of input parameter
	; 'delim' with one 'newdelim' in parameter 'str' so that you can
	; use $piece on the result.
	; This lets us use $piece like AWK does
	new dlen,i,lastch,len,next,output,substr
	; convert tabs to spaces when delim is not specified
	if $length($get(delim))=0 set str=$translate(str,$char(9),$char(32))
	set newdelim=$get(newdelim,$char(32))	; default to space
	set delim=$get(delim,$char(32))		; default to space
	set len=$zlength(str),lastch=1,dlen=$zlength(delim)
	; $zfind to the first occurrence of delim after lastch
	for i=1:1  quit:lastch>len  set next=$zfind(str,delim,lastch) quit:'next  do
	.	; append non-null extract of str from lastch to next
	.	set substr=$zextract(str,lastch,next-(1+dlen))
	.	if $zlength(substr) set output=$get(output)_substr_newdelim
	.	; advance until the next non-delim character
	.	for lastch=next:1:(len+1) quit:($zextract(str,lastch)'=delim)
	; append the remainder of str
	if lastch<(len+1) set output=$get(output)_$zextract(str,lastch,len)
	quit $get(output)
	; split a string into an array like AWK split does
SPLIT(str,delim)
	new outstr,i
	set outstr=$$^%MPIECE(str,$get(delim,$char(32)),$char(0))
	for i=1:1:$zlength(outstr,$char(0)) set fields(i)=$zpiece(outstr,$char(0),i)
	if $data(fields)<10 set $ECODE=",U117,"
	quit *fields
