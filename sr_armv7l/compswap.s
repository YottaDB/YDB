/* compswap.s */
	
/*    boolean_t compswap(sm_global_latch_ptr_t latch, int compval, int newval)
 *	If the supplied latch matches the comparison value, the new value
 *	is stored in the latch atomically.
 *     Return TRUE if swap/store succeeds,otherwise return FALSE.
 *
 *		r0 - Pointer to latch
 *		r1 - comparison value
 *		r2 - replacement value
 */

	.title	compswap.s
	.sbttl	compswap
	
.include "linkage.si"

	.text

RETRY_COUNT	= 32
	
/*
 * Note since this routine makes no calls, stack alignment is not critical. If ever a call is added then this
 * routine should take care to align the stack to 8 bytes and add a CHKSTKALIGN macro.
 */
	
ENTRY compswap
	mov	r12, #RETRY_COUNT
	dmb
retry:
	ldrex	r3, [r0]		/* get latch value */
	cmp	r3, r1
	bne	nomatch
	strexeq	r3, r2, [r0]		/* only do swap if latch value matches comparison value */
	cmpeq	r3, #1
	beq	notset			/* store-exclusive failed */
	movs	r0, #1			/* return success */
	dmb
	bx	lr
nomatch:
	clrex				/* reset exclusive status */
notset:
	subs	r12, #1
	bgt	retry
	movs	r0, #0			/* return failure */
	bx	lr

.end
