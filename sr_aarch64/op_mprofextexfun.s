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

/* op_mprofextexfun.s */

/*
 * op_extexfun calls an external GT.M MUMPS routine with arguments and provides for
 * a return value in most instances. If the routine has not yet been linked into the
 * current image, op_extexfun will first link it by invoking the auto-ZLINK function.
 * Before driving the function, we check if *any* routines have been linked in as
 * autorelink-enabled or if any directories are autorelink-enabled and if so, drive
 * a check to see if a newer version exists that could be linked in.
 *
 * Parameters:
 *
 *  - arg0 (x0) - Index into linkage table of caller containing the routine header
 *	          address of the routine to call (rtnidx).
 *  - arg1 (w1) - Index into linkage table of caller containing the address of an
 *		  offset into routine to which to transfer control associated with
 *	          a given label. This value is typically the address of the lnr_adr
 *		  field in a label entry (lblidx).
 *  - arg2 (x2) - Address of where return value is placed or NULL if none (ret_value).
 *  - arg3 (w3) - Bit mask with 1 bit per argument (ordered low to high). When bit is
 *		  set, argument is pass-by-value, else pass-by-reference (mask).
 *  - arg4 (w4)	- Count of M routine parameters supplied (actualcnt).
 *  - arg5 (x5)	- Address of first mval parameters (actual1).
 *  - arg6 (x6)	- Address of second mval parameters (actual2).
 *  - arg7 (x7)	- Address of tghird mval parameters (actual3).
 * The rest of the args are on the stack
 *  - actual4   - Address of the fourth mval parameter
 *  - actual5   - Address of the fifth mval parameter
 *  - . . .
 *
 * Note if lblidx (arg1) is negative, this means the linkage table to use is not from the
 * caller but is contained in TREF(lnk_proxy) used by indirects and other dynamic
 * code (like callins).
 *
 * Note we use w22 as a flag that we don't do more than one of auto_zlink() OR auto_relink_check()
 * call. This keeps us out of any possible loop condition as only one or the other should
 * ever be necessary. Register w22 is also known as REG_LITERAL_BASE and is saved by the putframe
 * macro so we need not save it separately.
 */

	.include "linkage.si"
	.include "g_msf.si"
	.include "gtm_threadgbl_deftypes_asm.si"
	.include "stack.si"
#	include "debug.si"

	.data
	.extern	dollar_truth
	.extern	frame_pointer
	.extern gtm_threadgbl

	.text
	.extern	auto_zlink
	.extern auto_relink_check
	.extern	new_stack_frame_sp
	.extern	push_parm
	.extern	rts_error
	.extern laberror

actual3		=	-40					/* x7 */
actual2		=	-32					/* x6 */
actual1		=	-24					/* x5 */
act_cnt		=	-16					/* w4 */
mask_arg	=	-12					/* w3 */
ret_val		=	 -8					/* x2 */
SAVE_SIZE	=	 48					/* Multiple of 16 to ensure stack alignment */

ENTRY op_mprofextexfun
	putframe						/* Save registers into current M stack frame */
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp						/* Copy stack pointer to r11 (x29) */
	sub	sp, sp, #SAVE_SIZE				/* establish space for locals - still 8 byte aligned */
	CHKSTKALIGN						/* Verify stack alignment */
	mov	x22, xzr					/* Init flag - We haven't done auto_zlink/auto_relink_check */
	sxtw	x1, w1						/* Sign extend argq1 so can check for negative arg */
	mov	x28, x1						/* Save index args */
	mov	x27, x0
	/*
	 * Note from here down, do *not* use REG_FRAME_POINTER which was overwritten above. REG_FRAME_POINTER  is an alias for
	 * register x29 which contains a copy of sp before sp was decremented by the save area size so x29 contains a
	 * pointer. Just past the save area we've allocated which is why all references are using negative offsets.
	 */
	str	x2, [x29, #ret_val]
	str	w3, [x29, #mask_arg]
	str	w4, [x29, #act_cnt]
	str	x5, [x29, #actual1]
	str	x6, [x29, #actual2]
	str	x7, [x29, #actual3]
	/*
	 * First up, check the label index to see if tiz negative. If so, we must use lnk_proxy as a base address
	 * and pseudo linkagetable. Else use the caller's linkage table.
	 */
	cmp	x1, xzr						/* Use current frame linkage table or lnk_proxy? */
	b.ge	loadandgo
	/*
	 * We have a negative index. Use lnk_proxy as a proxy linkage table.
	 */
	ldr	x2, =gtm_threadgbl
	ldr	x2, [x2]					/* x2 contains threadgbl base */
	ldr	x3, =ggo_lnk_proxy
	add	x3, x3, x2					/* -> lnk_proxy.rtnhdr_adr */
	cbnz	x0, gtmcheck
	ldr	x0, [x3]					/* -> rtnhdr */
	cbz	x0, gtmcheck					/* If rhdaddr == 0, not yet linked into image which */
								/* .. should never happen for indirects */
	cmp	x1, #-1						/* Using proxy table, label index must be -1 */
	b.ne	gtmcheck
	ldr	x15, [x3, #8]					/* ->label table code offset ptr */
	cbz	x15, gtmcheck					/* If labaddr == 0 && rhdaddr != 0, label does not exist */
								/* .. which also should never happen for indirects */
	ldr	x12, [x3, #16]					/* See if a parameter list was supplied */
	cbz	x12, fmllstmissing				/* If not, raise error */

	b	justgo						/* Bypass autorelink check for indirects (done by caller) */
	/*
	 * We have a non-negative index. Use args as indexes into caller's linkage table.
	 */
loadandgo:
	ldr	x12, [x19]					/* -> frame_pointer */
	ldr	x3, [x12, #msf_rvector_off]			/* -> frame_pointer->rvector (rtnhdr) */
	ldr	x3, [x3, #mrt_lnk_ptr]				/* -> frame_pointer->rvector->linkage_adr */
	lsl	x0, x0, #3					/* arg * 8 = offset for rtnhdr ptr */
	add	x2, x0, x3
	ldr	x2, [x2]
	cbz	x2, autozlink					/* Not defined - try auto-zlink */
	mov	x0, x2						/* -> rtnhdr */
	/*
	 * Have rtnhdr to call now. If rtnhdr->zhist, we should do an autorelink check on this routine to see if it needs
	 * to be relinked. Only do this if w22 is 0 meaning we haven't already done an autorelink check or if we just
	 * loaded the routine via auto_zlink.
	 */
	cbnz	x22, getlabeloff				/* Already checked-bypass this check and resolve the label offset */
	ldr	x4, [x0, #mrt_zhist]				/* See if we need to do an autorelink check */
	cbnz	x4, autorelink_check				/* Need autorelink check */
getlabeloff:
	lsl	x1, x1, #3					/* arg * 8 = offset for label offset ptr */
	add	x2, x1, x3
	ldr	x2, [x2]					/* See if defined */
	cbz	x2, label_missing
	mov	x1, x2						/* -> label table code offset */
	ldr	w15, [x1, #8]
	cbz	w15, fmllstmissing				/* If has_parms == 0, then issue an error */
	ldr	x15, [x1]					/* &(code_offset) for this label (usually & of lntabent) */
	/*
	 * Create stack frame and invoke routine
	 */
justgo:
	cbz	x15, label_missing
	ldr	w2, [x15]					/* Code offset for this label */
	ldr	x1, [x0, #mrt_ptext_adr]
	add	x2, x1, w2, sxtw				/* Transfer address: codebase reg + offset to label */
	ldr	x1, [x0, #mrt_lnk_ptr]				/* Linkage table address (context pointer) */
	bl	new_stack_frame_sp
	/*
	 * Move parameters into place
	 */
	ldr	w0, [x29, #act_cnt]				/* number of actuallist parms */
	cbz	w0, no_arg
	cmp	w0, #1
	b.eq	one_arg
	cmp	w0, #2
	b.eq	two_arg
	ADJ_STACK_ALIGN_EVEN_ARGS w0
	mov	x11, sp						/* Use x11 for stack copy since 8 byte alignment is bad for sp */
	sub	w9, w0, #3					/* Number of arguments to copy (number already pushed on stack) */
	cmp	w0, #3
	b.eq	three_arg
	add	x10, x29, x9, LSL #3				/* where to copy from -- skip 8 * (count - 1) + 16 */
	add	x10, x10, #8					/* The 16 is for the pushed x30 and x29, so 8 * count + 8 */
loop:
	ldr	x12, [x10], #-8
	str	x12, [x11, #-8]!
	subs	w9, w9, #1
	b.ne	loop
	mov	sp, x11						/* Copy is done, now we can use sp */
	CHKSTKALIGN						/* Verify stack alignment */
three_arg:
	ldr	x7, [x29, #actual3]
two_arg:
	ldr	x6, [x29, #actual2]
one_arg:
	ldr	x5, [x29, #actual1]
no_arg:
	ldr	w4, [x29, #act_cnt]
	ldr	w3, [x29, #mask_arg]
	ldr	x2, [x29, #ret_val]				/* ret_value */
	ldr	w1, [x25]
	and	w1, w1, #1
	add	w0, w0, #4					/* include total count, $T, addr_ret_val, mask */
	bl	push_parm					/* push_parm(total_cnt, $T, ret_value, mask, argc [,arg1, arg2, ...]) */
retlab:
	mov	sp, x29						/* Unwind C stack back to caller (almost) */
	ldp	x29, x30, [sp], #16
	getframe						/* Load regs from top M frame (sets return address in x30) */
	mov	sp, x29						/* Now stack is completely unwound */
	ret
/*
 * Drive auto_zlink to fetch module
 */
autozlink:
	cbnz	x22, gtmcheck					/* Already did autorelink or autorelink check? */
	mov	x0, x27						/* Get index arg back */
	bl	auto_zlink
	mov	x0, x27						/* Restore both args after call */
	mov	x1, x28
	mov	x22, #1
	b	loadandgo
/*
 * Drive auto_relink_check to see if a newer routine should be loaded
 */
autorelink_check:
	cbnz	x22, gtmcheck					/* Already did autorelink or autorelink check? */
	mov	x1, x28						/* Restore both args as parms for call */
	mov	x0, x27
	bl	auto_relink_check
	mov	x1, x28						/* Restore both args after call */
	mov	x0, x27
	mov	x22, #2
	b	loadandgo
/*
 * Raise GTMCHECK (pseudo-GTMASSERT since args are more difficult in assembler) when something really screwedup
 * occurs
 */
gtmcheck:
	ldr	x1, =ERR_GTMCHECK
	mov	x0, #1
	bl	rts_error
	b	retlab
/*
 * Make call so we can raise the appropriate LABELMISSING error for the not-found label.
 */
label_missing:
	mov	x0, x28						/* Index to linkage table and to linkage name table */
	bl	laberror
	b	retlab
/*
 * Raise missing formal list error
 */
fmllstmissing:
	ldr	x1, =ERR_FMLLSTMISSING
	mov	x0, #1
	bl	rts_error
	b	retlab
