/* opp_indfnname.s */

/*
 * void	op_indfnname(mval *dst, mval *target, mval *depth)
 */

	.title	opp_indfnname.s

.include "linkage.si"
.include "g_msf.si"
.include "debug.si"

	.sbttl	opp_indfnname

	.data
.extern frame_pointer

	.text
.extern	op_indfnname
	
ENTRY opp_indfnname
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_indfnname
	getframe
	bx	lr

.end
