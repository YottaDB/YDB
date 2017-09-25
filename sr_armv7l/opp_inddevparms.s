/* opp_inddevparms.s */

/*
 * void	op_inddevparms(mval *devpsrc, int4 ok_iop_parms,  mval *devpiopl)
 */

	.title	opp_inddevparms.s

.include "linkage.si"
.include "g_msf.si"
.include "debug.si"

	.sbttl	opp_inddevparms

	.data
.extern frame_pointer

	.text
.extern	op_inddevparms

 
ENTRY opp_inddevparms
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_inddevparms
	getframe
	bx	lr


.end
