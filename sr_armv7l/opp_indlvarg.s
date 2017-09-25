/* opp_indlvarg.s */

/*
 * void	op_indlvarg(mval *v, mval *dst)
 */

	.title	opp_indlvarg.s

.include "linkage.si"
.include "g_msf.si"
.include "debug.si"

	.sbttl	opp_indlvarg

	.data
.extern	frame_pointer

	.text
.extern	op_indlvarg

 
ENTRY opp_indlvarg
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_indlvarg
	getframe
	bx	lr


.end
