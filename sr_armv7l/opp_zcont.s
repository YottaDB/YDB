/* opp_zcont.s */

/*
 * void	op_zcont(void)
 */

	.title	opp_zcont.s

.include "linkage.si"
.include "g_msf.si"
.include "debug.si"

	.sbttl	opp_zcont

	.data
.extern	frame_pointer

	.text
.extern	op_zcont

 
ENTRY opp_zcont
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_zcont
	getframe
	bx	lr


.end
