#################################################################
#								#
# Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	#
# All rights reserved.						#
#								#
# Copyright (c) 2018 Stephen L Johnson. All rights reserved.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

/* op_sto.s */

	/* x0 is mval of destination */
	/* x1 is mval of value to store in destination */

	.include "linkage.si"
	.include "mval_def.si"
#	include "debug.si"

	.data
	.extern	literal_null
	.extern	undef_inhibit

	.text
	.extern	underr

ENTRY op_sto
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	CHKSTKALIGN					/* Verify stack alignment */
	mov	x12, x0					/* Save x0 */
	mv_if_notdefined x1, notdef
nowdef:
	mov	w15, #mval_byte_len
	mov	x0, x12					/* Restore x0 */
loop16:
	ldp	x9, x10, [x1], #16
	stp	x9, x10, [x0], #16
	subs	w15, w15, #16
	b.gt	loop16

	mov	x0, #mval_m_aliascont
	mvn	x0, x0					/* bitwise NOT of x0 */
	ldrh	w1, [x12, #mval_w_mvtype]
	and	x0, x0, x1
	strh	w0, [x12, #mval_w_mvtype]
done:
	mov	sp, x29
	ldp	x29, x30, [sp], #16
	ret
notdef:
	ldr	x0, =undef_inhibit
	ldrb	w0, [x0]
	cbz	w0, clab
	ldr	x1, =literal_null
	b	nowdef
clab:
	mov	x0, x1
	bl	underr
	b	done
