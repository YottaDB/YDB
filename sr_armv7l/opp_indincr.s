/* opp_indincr.s */

/*
 * void	op_indincr(mval *dst, mval *increment, mval *target)
 */

	.title	opp_indincr.s

.include "linkage.si"
.include "g_msf.si"
.include "debug.si"

	.sbttl	opp_indincr

	.data
.extern frame_pointer

	.text
.extern	op_indincr
	
 
ENTRY opp_indincr
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_indincr
	getframe
	bx	lr


.end
