/* op_linestart.s */
	
	.title	op_linestart.s

.include "linkage.si"
.include "g_msf.si"
.include "stack.si"
.include "debug.si"

	.sbttl	op_linestart

	.data
.extern	frame_pointer

	.text
	
/*
 * Routine to save the current return address and context in the current stack frame.
 *
 * Since this routine is a leaf routine (no calls), its stack frame alignment is not critical. If that changes,
 * this routine should do the necessary to keep the stack 8 byte aligned and use the CHKSTKALIGN macro to verify
 * it is so.
 */
ENTRY op_linestart
	ldr	r12, [r5]
	str	lr, [r12, #msf_mpc_off]				/* save incoming return address in frame_pointer->mpc */
	str	r6, [r12, #msf_ctxt_off]			/* save ctxt in frame_pointer */
	bx	lr

.end
