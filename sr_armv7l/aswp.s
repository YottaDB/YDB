/* aswp.s */
/* Atomic swap operation
 *	int aswp(sm_int_ptr_t *loc, int value)
 *	atomically set *loc to value and return the old value of *loc
 *
 *		r0 - pointer to loc
 *		r1 - value
*/

	.title	aswp.s
	.sbttl	aswp
	
.include "linkage.si"

	.text

RETRY_COUNT	=	1024
	
/*
 * Note since this routine makes no calls, stack alignment is not critical. If ever a call is added then this
 * routine should take care to align the stack to 8 bytes and add a CHKSTKALIGN macro.
 */
ENTRY aswp
	mov	r12, #RETRY_COUNT
	dmb
retry:
	ldrex	r2, [r0]
	strex	r3, r1, [r0]
	cmp	r3, #1
	beq	store_failed
	dmb
	mov	r0, r2
	bx	lr
store_failed:
	subs	r12, #1
	bne	retry
	mov	r12, #RETRY_COUNT
	b	retry
.end

