;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 2005 Fidelity Information Services, Inc	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;


; Compare and Swap
;  int compswap(gtm_global_latch_t *loc, int cmpvalue1, int cmpvalue2, int newvalue1, int newvalue2)
;
; For VMS, the latch is the form of an aligned 8 byte field. The comparison value is
; constructed from (cmpvalue2 << 32 | cmpvalue1) and likewise the swap value
; is constructed from (newvalue2 << 32 | cmpvalue1). If the supplied latch contains
; the constructed comparison value, the constructed swap value is stored in the latch atomically.
; Return TRUE if store succeeds and lock reset. Otherwise return FALSE.

.IF DEFINED GTMSHR_COMPSWAP

	.title	COMPSWAP

	$routine COMPSWAP, entry=COMPSWAP_CA, kind=null
.ELSE
.IF DEFINED GTMSECSHR_COMPSWAP

	.title	COMPSWAP_SECSHR

	$routine COMPSWAP_SECSHR, entry=COMPSWAP_SECSHR_CA, kind=null
.ELSE
.ERROR "Unsupported image"
.ENDC
.ENDC

RETRY_COUNT = 1000	; of times to attempts before sleeping
SLEEP_TIME = 500	; 1/2 ms
TRUE = 1
FALSE = 0
stack_size = 56

	lda	sp, -stack_size(sp)
	stq	r26, 0(sp)
	stq	r16, 8(sp)			; ->loc
	stq	r17, 16(sp)			; cmpvalue1
	stq	r18, 24(sp)			; cmpvalue2
	stq	r19, 32(sp)			; newvalue1
	stq	r20, 40(sp)			; newvalue2
	.base	r27, $ls

	mov	RETRY_COUNT, r24

; Make sure we know what we are about to pick up.
	mb

; the ldl_l/stl_c combination detects whether the location has been modified
; in between load and store.
	mov	FALSE,r0			; assume failure
retry:
	sll	r18, 32, r21			; r21 = r18 << 32
	bis	r21, r17, r21			; add in low order part of comparison value
	ldq_l	r22, (r16)			; load and lock the value
	cmpeq	r22, r21, r23			; expected value supplied?
	beq	r23, return_now			; return fact that compare failed
	sll	r20, 32, r21			; create 8 byte swap value
	bis	r21, r19, r21
	stq_c	r21, (r16)			; store and return
; r22 == 0:  unsuccessful, retry operation
; r22 == 1:  successful swap
	beq	r21, store_failed

; Make sure what we put down is really there..
	mb
	mov	TRUE,r0				; Store was a success
return_now:					; both error and normal return
	ldq	r26,0(sp)
	lda	sp, stack_size(sp)
	ret	(r26)


; retry operation immediately unless we have retried too many times.  In that
; case hibernate for a short period..
store_failed:
	subq	r24, 1, r24
	bne	r24, retry
.IF DEFINED GTMSHR_COMPSWAP
; for GTMSECSHR, we do not sleep - not a good idea in privileged mode
	mov	SLEEP_TIME, r16
	$call	HIBER_START, args=<r16>, set_arg_info=false, nonstandard=true
.ENDC
	mov	RETRY_COUNT,r24
	ldq	r16, 8(sp)
	ldq	r17, 16(sp)
	ldq	r18, 24(sp)
        ldq	r19, 32(sp)
        ldq	r20, 40(sp)
	mov	FALSE, r0
	br	retry

	$end_routine

	.end
