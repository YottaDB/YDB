/* opp_break.s */

/*
 * void op_break(void)
 */

	.title	opp_break.s


.include "linkage.si"
.include "g_msf.si"
.include "debug.si"

	.sbttl	opp_break

	.data
.extern frame_pointer

	.text
.extern	op_break
	
 
ENTRY opp_break
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_break
	getframe
	bx	lr

.end
