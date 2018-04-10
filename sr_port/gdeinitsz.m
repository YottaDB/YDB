;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;
; Note this file should not be manually invoked
;
	Write "This routine (",$TEXT(+0),") should not be manually invoked",!
	Quit
;
; Entry point used by gdeinit.m to define YottaDB structure fields
;
Init
	; YottaDB structure and field definitions
	Set SIZEOF("gvstats")=472
	Quit
