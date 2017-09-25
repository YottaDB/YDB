/* op_startintrrpt.s */
	
	.title	op_startintrrpt.s

.include "linkage.si"
.include "g_msf.si"
.include "debug.si"

	.sbttl	op_startintrrpt

	.data
.extern	frame_pointer
.extern	neterr_pending

	.text
.extern	gvcmz_neterr
.extern	async_action
.extern	outofband_clear


ENTRY op_startintrrpt
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	ldr	r0, =neterr_pending
	ldrb	r0, [r0]
	cmp	r0, #0
	beq	l1
	bl	outofband_clear
	mov	r0, #0
	bl	gvcmz_neterr
l1:	mov	r0, #1
	bl	async_action
	getframe
	bx	lr


.end
