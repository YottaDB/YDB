;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 2005 Fidelity Information Services, Inc	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
; 	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; Atomic swap operation
;  int aswp(int *loc, int value)
;
; set *loc to value and return the old value of *loc,
; in the equivalent of one atomic operation.
;

.IF DEFINED GTMSHR_ASWP

	.title	ASWP

	$routine ASWP, entry=ASWP_CA, kind=null
.ELSE
.IF DEFINED GTMSECSHR_ASWP

	.title	ASWP_SECSHR

	$routine ASWP_SECSHR, entry=ASWP_SECSHR_CA, kind=null
.ELSE
.ERROR "Unsupported Image"
.ENDC
.ENDC

RETRY_COUNT = 1024	; of times to attempt swap before sleeping
SLEEP_TIME = 500	; 1/2 ms
stack_size = 32

	lda	sp, -stack_size(sp)
	stq	r26,0(sp)
	stq	r16,8(sp)			; loc
	stq	r17,16(sp)			; value
	.base	r27, $ls

	mov	RETRY_COUNT,r23

; Make sure we know what we're about to pick up.
	mb

; the ldl_l/stl_c combination detects whether the location has been modified
; in between load and store.
retry:	ldl_l	r22,(r16)
	mov	r22, r0
	mov	r17, r22
	stl_c	r22,(r16)
; r22 == 0:  unsuccessful, retry operation
; r22 == 1:  successful swap
	beq	r22,store_failed

; Make sure what we put down is really there..
	mb

; Return...
	ldq	r26,0(sp)
	lda	sp, stack_size(sp)
	ret	(r26)

; retry operation immediately unless we've retried too many times.  In that
; case hibernate for a short while defined above.
store_failed:
	subq	r23,1,r23
	bne	r23,retry
.IF DEFINED GTMSHR_ASWP
; for GTMSECSHR, we do not sleep - not a good idea in privileged mode
	mov	SLEEP_TIME,r16
	$call	HIBER_START, args=<r16>, set_arg_info=false, nonstandard=true
.ENDC
	mov	RETRY_COUNT,r23
	ldq	r16,8(sp)
	ldq	r17,16(sp)
	br	retry

	$end_routine

	.end
