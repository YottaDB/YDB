#################################################################
#								#
# Copyright (c) 2017 YottaDB LLC. and/or its subsidiaries.	#
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

/* op_sto.s */

	/* r0 is mval of destination */
	/* r1 is mval of value to store in destination */

	.title	op_sto.s
	.sbttl	op_sto

	.include "linkage.si"
	.include "mval_def.si"
#	include "debug.si"

	.data
	.extern	literal_null
	.extern	undef_inhibit

	.text
	.extern	underr

ENTRY op_sto
	push	{r12, lr}
	CHKSTKALIGN					/* Verify stack alignment */
	mov	r3, r0					/* Save r0 */
	mv_if_notdefined r1, notdef
nowdef:
	mov	r12, #mval_byte_len
	mov	r0, r3					/* Restore r0 */
loop:
	ldr	r4, [r1], #+4
	str	r4, [r0], #+4
	subs	r12, #4
	bgt	loop

	mov	r0, #mval_m_aliascont
	mvn	r0, r0					/* bitwise NOT of r0 */
	ldrh	r1, [r3, #mval_w_mvtype]
	and	r0, r1
	strh	r0, [r3, #mval_w_mvtype]
done:
	pop	{r12, pc}
notdef:
	ldr	r0, =undef_inhibit
	ldrb	r0, [r0]
	cmp	r0, #0
	beq	clab
	ldr	r1, =literal_null
	b	nowdef
clab:
	mov	r0, r1
	bl	underr
	b	done

	.end
