#################################################################
#								#
# Copyright (c) 2017-2024 YottaDB LLC and/or its subsidiaries.	#
# All rights reserved.						#
#								#
# Copyright (c) 2017 Stephen L Johnson. All rights reserved.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

/* opp_bxrelop_pattern.s */

	.title	opp_bxrelop_pattern.s
	.sbttl	opp_bxrelop_pattern

	.include "linkage.si"
	.include "stack.si"
#	include "debug.si"

	.text
	.extern	op_bxrelop_pattern

# Note: op_bxrelop_pattern() expects 5 arguments. But only 4 arguments are passed in registers. The 5th argument
# coming into opp_bxrelop_pattern.s is passed through the caller's stack. Since we do some stack manipulations locally (`push {fp,lr}`)
# the arguments coming in the caller's stack are not usable when we call op_bxrelop_pattern(). Therefore we need to copy them
# over into the current stack frame. Hence the need for the arg5_save and FRAME_SIZE macros.
arg5_save	= 0
FRAME_SIZE	= 8		/* Keep 8 bytes of save area even though only 4 bytes are needed (only 4 bytes needed for arg5)
				 * since stack needs to be 8-byte aligned.
				 */

ENTRY opp_bxrelop_pattern
	push	{fp, lr}
	mov	fp, sp
	sub	sp, #FRAME_SIZE			/* Allocate save area */
	CHKSTKALIGN				/* Verify stack alignment */
	# Note: r12 is a scratch register so it is used to temporarily copy stuff from caller stack frame to current stack frame
	ldr	r12, [fp, #8]			/* Access 5th argument from caller stack frame */
	str	r12, [sp]			/* Store as 5th argument in current stack frame */
	bl	op_bxrelop_pattern
	cmp	r0, #0
	mov	sp, fp
	pop	{fp, pc}

	.end

# Below line is needed to avoid the ELF executable from ending up with an executable stack marking.
.section        .note.GNU-stack,"",@progbits
