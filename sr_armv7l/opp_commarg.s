/* opp_commarg.s */

/*
 * void	op_commarg(mval *v, unsigned char argcode)
 */

	.title	opp_commarg.s

.include "linkage.si"
.include "g_msf.si"
.include "debug.si"

	.sbttl	opp_commarg

	.data
.extern frame_pointer

	.text
.extern	op_commarg


ENTRY opp_commarg
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_commarg
	getframe
	bx	lr
	

.end
