/* op_forintrrpt.s */

	
	.title	op_forintrrpt.s
	.sbttl	op_forintrrpt

.include "linkage.si"
.include "debug.si"

	.data
.extern	neterr_pending
.extern	restart_pc

	.text
.extern	gvcmz_neterr
.extern	async_action
.extern	outofband_clear

 
ENTRY op_forintrrpt
	push	{r4, lr}			/* r4 is to maintain 8 byte stack alignment */
	CHKSTKALIGN				/* Verify stack alignment */
	ldr	r2, =neterr_pending
	ldrb	r0, [r2]
	cmp	r0, #0
	beq	l1
	bl	outofband_clear
	mov	r0, #0
	bl	gvcmz_neterr
l1:	mov	r0, #0
	bl	async_action
	pop	{r4, pc}


.end
