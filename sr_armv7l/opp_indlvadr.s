/* opp_indlvadr.s */

/*
 * void	op_indlvadr(mval *target)
 */

	.title	opp_indlvadr.s

.include "linkage.si"
.include "g_msf.si"
.include "debug.si"

	.sbttl	opp_indlvadr

	.data
.extern	frame_pointer

	.text
.extern	op_indlvadr

 
ENTRY opp_indlvadr
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_indlvadr
	getframe
	bx	lr


.end
