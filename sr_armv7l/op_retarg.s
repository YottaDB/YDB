/* op_retarg.s */
/*
 *	r0 - pointer to mval being returned
 *	r1 - True/False for alias return
 */

	
	.title	op_retarg.s

.include "linkage.si"
.include "g_msf.si"
.include "stack.si"
.include "debug.si"

	.sbttl	op_retarg

	.data
.extern	frame_pointer

	.text
.extern	unw_retarg

 
ENTRY op_retarg
	CHKSTKALIGN					/* Verify stack alignment */
	bl	unw_retarg
	getframe
	bx	lr

.end
