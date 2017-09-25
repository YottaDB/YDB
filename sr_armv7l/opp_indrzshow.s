/* opp_indrzshow.s */

/*
 * void op_indrzshow(mval *s1, mval *s2)
 */

	.title	opp_indrzshow.s

.include "linkage.si"
.include "g_msf.si"
.include "debug.si"

	.sbttl	opp_indrzshow

	.data
.extern	frame_pointer

	.text
.extern	op_indrzshow

 
ENTRY opp_indrzshow
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_indrzshow
	getframe
	bx	lr


.end
