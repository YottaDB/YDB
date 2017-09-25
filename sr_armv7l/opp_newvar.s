/* opp_newvar.s */

/*
 * void op_newvar(uint4 arg1)
 */

	.title	opp_newvar.s

.include "linkage.si"
.include "g_msf.si"
.include "debug.si"

	.sbttl	opp_newvar

	.data
.extern	frame_pointer

	.text
.extern	op_newvar

 
ENTRY opp_newvar
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_newvar
	getframe
	bx	lr


.end
