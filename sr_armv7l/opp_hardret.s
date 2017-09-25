/* opp_hardret.s */

/*
 * void	op_hardret(void)
 */

	.title	opp_hardret.s

.include "linkage.si"
.include "g_msf.si"
.include "debug.si"

	.sbttl	opp_hardret

	.data
.extern	frame_pointer

	.text
.extern	op_hardret
	
 
ENTRY opp_hardret
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_hardret
	getframe
	bx	lr


.end
