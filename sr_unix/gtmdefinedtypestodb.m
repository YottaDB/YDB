;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
; Copyright (c) 2015-2017 Fidelity National Information		;
; Services, Inc. and/or its subsidiaries. All rights reserved.	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; This is called by generate_help.csh to put offset, length, type and format info into the gtmhelp database
GTMDEFINEDTYPESTODB
	set $etrap="use $principal write $zstatus,! set $etrap=""zgoto 0"" zhalt 1"
	do Init^GTMDefinedTypesInit
	merge ^gtmtypfldindx=gtmtypfldindx
	merge ^gtmstructs=gtmstructs
	merge ^gtmunions=gtmunions
	merge ^gtmtypes=gtmtypes
	quit
