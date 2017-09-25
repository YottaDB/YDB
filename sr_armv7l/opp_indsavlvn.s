/* opp_indsavlvn.s */

/*
 * void op_indsavlvn(mval *target, uint4 slot)
 */

	.title	opp_indsavlvn.s

.include "linkage.si"
.include "g_msf.si"
.include "debug.si"

	.sbttl	opp_indsavlvn

	.data
.extern	frame_pointer

	.text
.extern	op_indsavlvn

	
 
ENTRY opp_indsavlvn
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_indsavlvn
	getframe
	bx	lr
	

.end
