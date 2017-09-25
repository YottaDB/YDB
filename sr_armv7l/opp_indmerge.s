/* opp_indmerge.s */

/*
 * void op_indmerge(mval *glvn_mv, mval *arg1_or_arg2)
 */

	.title	opp_indmerge.s

.include "linkage.si"
.include "g_msf.si"
.include "debug.si"

	.sbttl	opp_indmerge

	.data
.extern	frame_pointer

	.text
.extern	op_indmerge

 
ENTRY opp_indmerge
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_indmerge
	getframe
	bx	lr


.end
