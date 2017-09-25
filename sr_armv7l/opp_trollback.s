/* opp_trollback.s */

/*
 * void	op_trollback(int rb_levels)
 */

	.title	opp_trollback.s

.include "linkage.si"
.include "g_msf.si"
.include "debug.si"

	.sbttl	opp_trollback

	.data
.extern	frame_pointer

	.text
.extern	op_trollback

 
ENTRY opp_trollback
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_trollback
	getframe
	bx	lr


.end
