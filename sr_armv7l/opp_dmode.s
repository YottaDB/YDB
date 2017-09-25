/* opp_dmode.s */

/*
 * void	op_dmode(void)
 */

	.title	opp_dmode.s

.include "linkage.si"
.include "g_msf.si"
.include "debug.si"

	.sbttl	opp_dmode

	.data
.extern	frame_pointer

	.text
.extern	op_dmode


ENTRY opp_dmode
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_dmode
	getframe
	bx	lr

.end
