/* opp_indtext.s */

/*
 * void op_indtext(mval *lab, mint offset, mval *rtn, mval *dst)
 */

	.title	opp_indtext.s

.include "linkage.si"
.include "g_msf.si"
.include "debug.si"

	.sbttl	opp_indtext

	.data
.extern	frame_pointer

	.text
.extern	op_indtext
	
 
ENTRY opp_indtext
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_indtext
	getframe
	bx	lr


.end
