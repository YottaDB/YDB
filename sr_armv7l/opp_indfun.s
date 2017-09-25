/* opp_indfun.s */

/*
 * void op_indfun(mval *v, mint argcode, mval *dst)
 */

	.title	opp_indfun.s

.include "linkage.si"
.include "g_msf.si"
.include "debug.si"

	.sbttl	opp_indfun
	
	.data
.extern frame_pointer

	.text
.extern	op_indfun

ENTRY opp_indfun
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_indfun
	getframe
	bx	lr


.end
