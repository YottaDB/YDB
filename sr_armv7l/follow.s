/* follow.s */
	
	.title	follow.s
	.sbttl	follow
	
.include "linkage.si"
.include "stack.si"
.include "debug.si"
	
	
	.text

.extern	op_follow

 
ENTRY follow
	push	{fp, lr}
	mov	fp, sp
	CHKSTKALIGN				/* Verify stack alignment */
	bl	op_follow
	ble	notfollow
	movs	r0, #1
	b	done
notfollow:
	movs	r0, #0
done:
	mov	sp, fp
	pop	{fp, pc}

.end
