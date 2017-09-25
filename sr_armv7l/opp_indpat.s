/* opp_indpat.s */

/*
 * void	op_indpat(mval *v, mval *dst)
 */

	.title	opp_indpat.s

.include "linkage.si"
.include "g_msf.si"
.include "debug.si"

	.sbttl	opp_indpat

	.data
.extern	frame_pointer

	.text
.extern	op_indpat

 
ENTRY opp_indpat
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_indpat
	getframe
	bx	lr


.end
