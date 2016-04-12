;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 2001, 2013 Fidelity Information Services, Inc	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
exit:	;implement the verb: EXIT
EXIT
	i 'update d QUIT^GDEQUIT
	i '$$ALL^GDEVERIF  zm gdeerr("NOEXIT")  q
	i '$$GDEPUT^GDEPUT  q	 ; zm is issued in GDEPUT.m
	d GETOUT^GDEEXIT
	h
GETOUT	; Routine executed just before exiting from GDE. This tries to restore the mumps process context
	;	to what it was at entry into GDE and then does a quit to the parent mumps program.
	; This context would have been saved in the "gdeEntryState" variable. It is possible this variable
	; is hidden due to argumentless/exclusive "new"s that happened inside GDE much after entry into GDE.
	; In that case, there is nothing available to do the restore so skip the restore and "halt" out of
	; the process to be safe (or else the parent mumps program could get confused).
	;
	i '$data(gdeEntryState) h
	n nullsubs
	s nullsubs=+gdeEntryState("nullsubs")
	u gdeEntryState("io"):(exception="")	; restore $io with no exception (as otherwise it would be CTRL^GDE)
	v $select(nullsubs=0:"NOLVNULLSUBS",nullsubs=1:"LVNULLSUBS",nullsubs=2:"NEVERLVNULLSUBS") ; restore LVNULLSUBS setting
	; Use unsubscripted variables for local collation related act,ncol,nct values as otherwise we could get
	; COLLDATAEXISTS error if we are restoring the local collation (before exit from GDE) as part of the $$set^%LCLCOL below.
	; For the same reason store zlevel info in an unsubscripted variable as it is needed for the zgoto at the end.
	n gdeEntryStateZlvl
	s gdeEntryStateZlvl=+gdeEntryState("zlevel")
	k (gdeEntryStateZlvl,gdeEntryStateAct,gdeEntryStateNcol,gdeEntryStateNct)
	i $$set^%LCLCOL(gdeEntryStateAct,gdeEntryStateNcol,gdeEntryStateNct) ; restores local variable collation characteristics
	zg gdeEntryStateZlvl ; this should exit GDE and return control to parent mumps process invocation
	h  ; to be safe in case control ever reaches here
