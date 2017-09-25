/* opp_zgoto.s */

/*
 * void op_zgoto(mval *rtn_name, mval *lbl_name, int offset, int level)
 */

	.title	opp_zgoto.s

.include "linkage.si"
.include "g_msf.si"
.include "debug.si"

	.sbttl	opp_zgoto

	.data
.extern	frame_pointer

	.text
.extern	op_zgoto

 
ENTRY opp_zgoto
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_zgoto
	getframe
	bx	lr


.end
