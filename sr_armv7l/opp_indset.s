/* opp_indset.s */

/*
 * void	op_indset(mval *target, mval *value)
 */

	.title	opp_indset.s

.include "linkage.si"
.include "g_msf.si"
.include "debug.si"

	.sbttl	opp_indset

	.data
.extern	frame_pointer

	.text
.extern	op_indset

 
ENTRY opp_indset
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_indset
	getframe
	bx	lr

.end
