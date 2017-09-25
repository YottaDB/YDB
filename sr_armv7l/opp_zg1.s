/* opp_zg1.s */

/*
 * void op_zg1(int4 level)
 */

	.title	opp_zg1.s

.include "linkage.si"
.include "g_msf.si"
.include "debug.si"

	.sbttl	opp_zg1

	.data
.extern	frame_pointer

	.text
.extern	op_zg1

 
ENTRY opp_zg1
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_zg1
	getframe
	bx	lr


.end
