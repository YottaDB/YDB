/* opp_iretmval.s */

/*
 * void op_iretmval(mval *v, mval *dst)
 */

	.title	opp_iretmval.s

.include "linkage.si"
.include "g_msf.si"
.include "debug.si"

	.sbttl	opp_iretmval

	.data
.extern	frame_pointer

	.text
.extern	op_iretmval

 
ENTRY opp_iretmval
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_iretmval
	getframe
	bx	lr

.end
