/* opp_newintrinsic.s */

/*
 * void op_newintrinsic(int intrtype)
 */

	.title	opp_newintrinsic.s

.include "linkage.si"
.include "g_msf.si"
.include "debug.si"

	.sbttl	opp_newintrinsic

	.data
.extern	frame_pointer

	.text
.extern	op_newintrinsic

 
ENTRY opp_newintrinsic
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_newintrinsic
	getframe
	bx	lr


.end
