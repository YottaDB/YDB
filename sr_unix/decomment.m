;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
; Copyright (c) 2010-2018 Fidelity National Information		;
; Services, Inc. and/or its subsidiaries. All rights reserved.	;
; 								;
; 	This source code contains the intellectual property	;
; 	of its copyright holder(s), and is made available	;
; 	under a license.  If you do not know the terms of	;
; 	the license, please stop and do not read further.	;
;	    	     	    	     	    	 		;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; Part of gengtmdeftypes.
;
; Routine to remove C comments from a C routine or header file. File to read is specified
; on command line (e.g "$gtm_dist/mumps -run file-to-uncomment.c"). Uncommented
; version of file is written to stdout and can be piped wherever. Note this is sufficient
; to decomment OUR (GT.M source code) code but may need additions to be truly general purpose.
;
	Set TRUE=1,FALSE=0
	;
	Set infile=$ZCmdline
	Do:(""=infile)
	. Write "Missing parm: file to operate on",!
	. Halt
	;
	; Initialization - simple token types
	;
	Set TKEOL=1		; End of line
	Set TKEOF=2		; End of file
	Set TKDQUOTE=3		; Double quote '"'
	Set TKCOMSTRT=4 	; Comment start "/*"
	Set TKCOMEND=5		; Comment end "*/"
	Set TKSLASH=6		; Potential piece of comment start/end
	Set TKASTERISK=7	; Potential piece of comment start/end
	Set TKBACKSLASH=8	; Character escape
	Set TKOTHER=9		; Everything else than what we care about
	;
	Set CharScn("*")=TKASTERISK
	Set CharScn("\")=TKBACKSLASH
	Set CharScn("""")=TKDQUOTE
	Set CharScn("/")=TKSLASH
	;
	; Open file make sure exists
	;
	Open infile:Readonly
	Use infile
	Set inline=1
	Set (CommentMode,QuoteMode,NextTokEol)=FALSE
	;
	; Prime the tokenizer pump
	;
	Set (inbuf,outbuf,token,dirtoken,tokenval,dirtokenval)=""
	Do GetToken(FALSE)	; Set tokens - neither of the "fake" tokens initialized earlier need flushing
	Do GetToken(FALSE)	; Set director token (next)
	For  Quit:TKEOF=token  Do
	. ;
	. ; If this char is supposed to be escaped because of a previous backslash, make it type TKOTHER so it is otherwise ignored
	. ; from a parsing standpoint..
	. ;
	. If TKEOL=token Do		; End of line - Next!
	. . Do GetToken(TRUE)
	. Else  If TKBACKSLASH=token,TKEOL'=dirtoken Do
	. . Do GetToken('CommentMode)
	. . ;
	. . ; We have a new char to inflict on ourselves. But this char has been "escaped" so any special meaning
	. . ; it would ordinarily have disappears - just set its token type to TKOTHER so it plays nice with everything else.
	. . ;
	. . Set:(TKEOF'=token) token=TKOTHER
	. Else  If 'QuoteMode,TKCOMSTRT=token Do	; Entering comment mode (so long as not already in quote mode)
	. . Do:(CommentMode) Error("INTERROR","F","Comment mode already on when entering comment mode")
	. . Set comstart=inline,CommentMode=TRUE
	. . Do GetToken(FALSE)
	. . For  Quit:((TKEOF=token)!(TKCOMEND=token))  Do
	. . . Do GetToken(FALSE)
	. . . Do:((TKBACKSLASH=token)&(TKEOL'=dirtoken))
	. . . . Do GetToken(FALSE)
	. . . . Set:((TKEOF'=token)&(TKEOL'=token)) token=TKOTHER
	. . Do:(TKEOF=token) Error("COMENDNF","E","Hit end of routine while looking for end of comment started at line "_comstart)
	. . Do GetToken(FALSE)
	. . Set CommentMode=FALSE
	. Else  If TKCOMEND=token Do Error("FNDCOMNICM","F","Located end-of-comment token in line "_inline_" while not in comment mode")
	. Else  If 'CommentMode,TKDQUOTE=token Do	; Entering quoted text mode (ignored if already in comment mode)
	. . Do:(QuoteMode) Error("INTERROR","F","Quote mode already on when entering quote mode")
	. . Set QuoteMode=TRUE
	. . Do GetToken(TRUE)
	. . Do:((TKBACKSLASH=token)&(TKEOL'=dirtoken))
	. . . Do GetToken(TRUE)
	. . . Set:((TKEOF'=token)&(TKEOL'=token)) token=TKOTHER
	. . Set done=FALSE
	. . ;
	. . ; On each loop iteration, there are two possibilities if a quote is detected:
	. . ;   1. If the director char is also a quote, then we do not leave quote mode but do push the scan pointer to
	. . ;      the char following the director quote.
	. . ;   2. If the director char is NOT a quote, quote mode is done.
	. . ;
	. . For  Quit:(done!(TKEOF=token)!(TKEOL=token))  Do
	. . . If TKDQUOTE=token Do
	. . . . ;
	. . . . ; We found a potentially quote ending quote. But if the next token is also a quote, then
	. . . . ; this is an escaped quote and not an ending quote.
	. . . . ;
	. . . . If TKDQUOTE=dirtoken Do
	. . . . . Do GetToken(TRUE)	; Not and ending quote, just eat the second one and continue scan
	. . . . . Do:((TKBACKSLASH=token)&(TKEOL'=dirtoken))
	. . . . . . Do GetToken(TRUE)
	. . . . . . Set:((TKEOF'=token)&(TKEOL'=token)) token=TKOTHER
	. . . . Else  Do
	. . . . . Set done=TRUE		; Else, this is an ending quote - set loop terminator
	. . . . . Set QuoteMode=FALSE	; .. and exit quote mode
	. . . Else  Do GetToken(TRUE)
	. . Do:(TKEOF=token) Error("CLOSQUOTNF","E","Routine ended while searching for closing quote")
	. . Do:(TKEOL=token) Error("LINENDQT","E","Line ("_inline_") ended while searching for closing quote")
	. . Do GetToken(TRUE)
	. Else  Do GetToken(TRUE)
	;
	Do:(""'=outbuf) FlushOutbuf	; Wee bit 'o cleanup
	Close inbuf,outbuf
	Quit

;
; Routine to "tokenize" the input file. These are not compiler-tokens but simplistic
; comment-detecting removal tokens so are much coarser grain. Not a lot of care about
; anything except that which allows comments across lines to be removed while dealing
; appropriately with quotes and escaped chars.
;
; Argument tells whether to flush the current token to the output buffer or not before
; it gets replaced.
;
GetToken(flush)
	New done,seenchr,prevtoken
	Set done=FALSE
	Quit:(TKEOF=token) TKEOF
	Do:(flush)		; Buffer output token and if ends line, write it
	. Set outbuf=outbuf_tokenval
	. Do:(TKEOL=token) FlushOutbuf
	Do:(TKEOF=dirtoken)
	. ;
	. ; Simple case where we are out of input
	. ;
	. Set token=TKEOF
	. Set tokenval=""
	. Set done=TRUE
	Quit:done
	;
	; Else we need the next token
	;
	Set prevtoken=token
	Set token=dirtoken
	Set tokenval=dirtokenval
	;
	; If our last inbuf reading was ended by endofline (NextTokEol is true) meaning we returned
	; whatever we got (if anything) before that, take care of that now.
	;
	Do:NextTokEol
	. Set dirtoken=TKEOL
	. Set dirtokenval=""
	. Set NextTokEol=FALSE
	. Set done=TRUE
	Quit:done

	;
	; Scan to create next director token/val
	;
	Set dirtokenval=""
	Do:(""=inbuf)
	. If $ZEof Do		; Oops, at EOF with nothing read
	. . Set dirtoken=TKEOF
	. . Set dirtokenval=""
	. . Set done=TRUE
	. Else  Do		; Read a new line - still might detect EOF but might get lucky too!
	. . Read inbuf
	. . If $ZEof Do
	. . . If ""=dirtokenval Set dirtoken=TKEOF
	. . . Set done=TRUE
	. . Set inline=inline+1
	;
	; Quick pre-check for simple blank (null) line if not already done
	;
	Do:('done&(""=inbuf))	; Null line is just a TKEOL return
	. Set dirtoken=TKEOL
	. Set dirtokenval=""
	. Set done=TRUE
	Quit:done		; Processing already complete - bypass scan
	;
	; Scan input line for token-ending chars
	;
	For scnp=1:1:$ZLength(inbuf) Quit:done  Do
	. Set chr=$ZExtract(inbuf,scnp)
	. Do:(0<$Data(CharScn(chr)))
	. . If TKASTERISK=CharScn(chr) Do		; Possible end of comment
	. . . Set chr2=$ZExtract(inbuf,scnp+1)
	. . . Do:(TKSLASH=$Get(CharScn(chr2),TKOTHER))
	. . . . ;
	. . . . ; We have an end-of-comment token. Stop the scan here if we have scanned chars to
	. . . . ; return the scanned part as TKOTHER. We will return the end of comment token for the
	. . . . ; next scan. If we have scanned nothing, then we return the end of comment token.
	. . . . ;
	. . . . If (1<scnp) Do				; Returning previous string as TKOTHER
	. . . . . Set dirtokenval=$ZExtract(inbuf,1,scnp-1)
	. . . . . Set dirtoken=TKOTHER
	. . . . . Set inbuf=$ZExtract(inbuf,scnp,99999)
	. . . . Else  Do
	. . . . . Set dirtokenval="*/"
	. . . . . Set dirtoken=TKCOMEND
	. . . . . Set inbuf=$ZExtract(inbuf,3,99999)
	. . . . . Set:(""=inbuf) NextTokEol=TRUE
	. . . . Set done=TRUE
	. . Else  If TKBACKSLASH=CharScn(chr) Do	; Escaped char coming up
	. . . ;
	. . . ; Same deal as above - return char if first char scanned, else return previously scanned.
	. . . ;
	. . . If (1<scnp) Do
	. . . . Set dirtokenval=$ZExtract(inbuf,1,scnp-1)
	. . . . Set dirtoken=TKOTHER
	. . . . Set inbuf=$ZExtract(inbuf,scnp,99999)
	. . . Else  Do
	. . . . Set dirtokenval="\"
	. . . . Set dirtoken=TKBACKSLASH
	. . . . Set inbuf=$ZExtract(inbuf,2,99999)
	. . . . Set:(""=inbuf) NextTokEol=TRUE
	. . . Set done=TRUE
	. . Else  If TKSLASH=CharScn(chr) Do		; Possible start of comment
	. . . Set chr2=$ZExtract(inbuf,scnp+1)
	. . . Do:(TKASTERISK=$Get(CharScn(chr2),TKOTHER))
	. . . . ;
	. . . . ; Same deal as above - return char if first char scanned, else return previously scanned.
	. . . . ;
	. . . . If (1<scnp) Do			; Returning previous string as TKOTHER
	. . . . . Set dirtokenval=dirtokenval_$ZExtract(inbuf,1,scnp-1)
	. . . . . Set dirtoken=TKOTHER
	. . . . . Set inbuf=$ZExtract(inbuf,scnp,99999)
	. . . . Else  Do
	. . . . . Set dirtokenval="/*"
	. . . . . Set dirtoken=TKCOMSTRT
	. . . . . Set inbuf=$ZExtract(inbuf,3,99999)
	. . . . Set done=TRUE
	. . Else  Do:(TKDQUOTE=$Get(CharScn(chr),TKOTHER))
	. . . ;
	. . . ; We have a quote token. Stop scan here if have scanned chars to return previous scan as
	. . . ; TKOTHER. We will return the quote token on the next scan. If we have scanned nothing,
	. . . ; return the start of comment token.
	. . . ;
	. . . If (1<scnp) Do			; Returning previous string as TKOTHER
	. . . . Set dirtokenval=dirtokenval_$ZExtract(inbuf,1,scnp-1)
	. . . . Set dirtoken=TKOTHER
	. . . . Set inbuf=$ZExtract(inbuf,scnp,99999)
	. . . Else  Do
	. . . . Set dirtokenval=""""
	. . . . Set dirtoken=TKDQUOTE
	. . . . Set inbuf=$ZExtract(inbuf,2,99999)
	. . . . Set:(""=inbuf) NextTokEol=TRUE
	. . . Set done=TRUE
	;
	; Coming out of the loop, two possibilities:
	;
	;   1. We are done, token set all is well - just return
	;   2. We ran out of text in the buffer after reading something - return text as TKOTHER
	;      setting NextTolEol so end-of-line is processed next call.
	;
	Quit:done	; Case 1
	;
	; Assumption here is we ran out of text - assert that
	;
	If $ZLength(inbuf)'=scnp Do Error("ASSERT","F","Did not scan full line yet loop ended without done set")
	Else  Do	; Case 2
	. Set dirtoken=TKOTHER
	. Set dirtokenval=inbuf
	. Set inbuf=""
	. Set NextTokEol=TRUE
	Quit

;
; Routine to flush output buffer (routine sans comments)
;
FlushOutbuf
	Use $Principal
	Write outbuf,!
	Set outbuf=""
	Use infile
	Quit

;
; Output error message - generate dump for fatal errors..
;
Error(msgid,severity,text)
	New zshowdmps
	Use $Principal
	Write !,"DECOMMENT-",severity,"-",msgid," ",text,!!
	Do:("F"=severity)
	. Set zshowdmps=$Increment(ZShowDumpsCreated)
	. Set dumpfile="decomment-fail.zshowdmp-"_$ZDate($Horolog,"YEARMMDD-2460SS")_"-"_zshowdmps_".txt"
	. Open dumpfile:New
	. Use dumpfile
	. ZShow "*"
	. Close dumpfile
	Halt

;
; Routines to enable debugging
;
dbgzwrite(zwrarg,sfx)
	New saveio
	Set saveio=$IO
	Use $Principal
	Write "DbgZwrite at ",$Stack($Stack-1,"PLACE"),":----------- ",$Select(""'=$Get(sfx,""):"("_sfx_")",TRUE:"")_":",!
	ZWrite @zwrarg
	Use saveio
	Quit

;
; Debugging routine..
;
dbgwrite(text)
	New saveio
	Set saveio=$IO
	Use $Principal
	Write text,!
	Use saveio
	Quit
