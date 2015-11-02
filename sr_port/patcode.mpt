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
%patcode	; ; ; Pattern Code Control
	;
get()	q $v("PATCODE")
	;
set(tab)
	n ok,$et
	s $et="s $ec="""" s ok=0",ok=1
	i $ZCHSET="UTF-8" s ok=0 ; user defined pattern code not supported in UTF-8. No error message given. Simply returns 0
	i ok v "PATCODE":tab
	q ok
