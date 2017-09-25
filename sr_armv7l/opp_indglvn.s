/* opp_indglvn.s */

/*
 * void	op_indglvn(mval *v,mval *dst)
 */

	.title	opp_indglvn.s

.include "linkage.si"
.include "g_msf.si"
.include "debug.si"

	.sbttl	opp_indglvn

	.data
.extern	frame_pointer

	.text
.extern	op_indglvn
	
 
ENTRY opp_indglvn
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_indglvn
	getframe
	bx	lr


.end
