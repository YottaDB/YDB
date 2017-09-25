/* op_mproflinestart.s */

/*
	op_mproflinestart - establish start of line in GT.M MUMPS stack frame
*/

	.title	op_mproflinestart.s

.include "linkage.si"
.include "g_msf.si"
.include "stack.si"
.include "debug.si"

	.sbttl	op_mproflinestart

	.data
.extern	frame_pointer

	.text
.extern	pcurrpos	
	

/*
 * This is the M profiling version which calls different routine(s) for M profiling purposes.
 */
ENTRY op_mproflinestart
	mov	r4, lr						/* Save link pointer in register, not on stack */
	CHKSTKALIGN						/* Verify stack alignment */
	ldr	r12, [r5]
	str	lr, [r12, #msf_mpc_off]
	str	r6, [r12, #msf_ctxt_off]			/* save ctxt into frame pointer */
	bl	pcurrpos
	bx	r4

.end
