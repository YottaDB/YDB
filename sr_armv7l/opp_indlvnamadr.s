/* opp_indlvnamadr.s */

/*
 * void	op_indlvnamadr(mval *target)
 */

	.title	opp_indlvnamadr.s

.include "linkage.si"
.include "g_msf.si"
.include "debug.si"

	.sbttl	opp_indlvnamadr

	.data
.extern	frame_pointer

	.text
.extern	op_indlvnamadr

 
ENTRY opp_indlvnamadr
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_indlvnamadr
	getframe
	bx	lr


.end
