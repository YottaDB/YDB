/* op_linefetch.s */
	
	.title	op_linefetch.s

.include "linkage.si"
.include "g_msf.si"
.include "stack.si"
.include "debug.si"

	.sbttl	op_linefetch

	.data
.extern	frame_pointer

	.text
.extern	gtm_fetch

ENTRY op_linefetch
	CHKSTKALIGN					/* Verify stack alignment */
	ldr	r12, [r5]
	str	lr, [r12, #msf_mpc_off]			/* save return address in frame_pointer->mpc */
	str	r6, [r12, #msf_ctxt_off]		/* save linkage pointer */
	bl	gtm_fetch
	ldr	r12, [r5]
	ldr	lr, [r12, #msf_mpc_off]
	bx	lr

.end
