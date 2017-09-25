/* opp_xnew.s */

/*
 * void op_xnew(unsigned int argcnt_arg, mval *s_arg, ...)
 *
 */

	.title	opp_xnew.s

.include "linkage.si"
.include "g_msf.si"
.include "stack.si"
.include "debug.si"

	.sbttl	opp_xnew

	.data
.extern	frame_pointer

	.text
.extern	op_xnew

ENTRY opp_xnew
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_xnew
	getframe
	bx	lr

.end
