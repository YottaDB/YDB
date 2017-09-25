/* opp_ret.s */

/*
 * void op_unwind(void)
 */

	.title	opp_ret.s

.include "linkage.si"
.include "g_msf.si"
.include "debug.si"

	.sbttl	opp_ret

	.data
.extern	frame_pointer

	.text
.extern	op_unwind

 
ENTRY opp_ret
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_unwind
	getframe
	bx	lr

.end
