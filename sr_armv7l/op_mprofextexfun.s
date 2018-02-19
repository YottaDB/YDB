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
 *  - arg0 (r0) - Index into linkage table of caller containing the routine header
 *	          address of the routine to call (rtnidx).
 *  - arg1 (r1) - Index into linkage table of caller containing the address of an
 *		  offset into routine to which to transfer control associated with
 *	          a given label. This value is typically the address of the lnr_adr
 *		  field in a label entry (lblidx).
 *  - arg2 (r2) - Address of where return value is placed or NULL if none (ret_value).
 *  - arg3 (r3) - Bit mask with 1 bit per argument (ordered low to high). When bit is
 *		  set, argument is pass-by-value, else pass-by-reference (mask).
 *  Pushed on stack
 *  - arg4	- Count of M routine parameters supplied (actualcnt).
 *  - arg5	- List of addresses of mval parameters (actual1).
 *
 * Note if lblidx (arg1) is negative, this means the linkage table to use is not from the
 * caller but is contained in TREF(lnk_proxy) used by indirects and other dynamic
 * code (like callins).
 *
 * Note we use r8 as a flag that we don't do more than one of auto_zlink() OR auto_relink_check()
 * call. This keeps us out of any possible loop condition as only one or the other should
 * ever be necessary. Register r8 is also known as REG_LITERAL_BASE and is saved by the putframe
 * macro so we need not save it separately.
 */

	.title	op_mprofextexfun.s

	.include "linkage.si"
	.include "g_msf.si"
	.include "gtm_threadgbl_deftypes_asm.si"
	.include "stack.si"
#	include "debug.si"

	.sbttl	op_mprofextexfun

	.DATA
	.extern	dollar_truth
	.extern	frame_pointer

	.text
	.extern	auto_zlink
	.extern	new_stack_frame_sp
	.extern	push_parm
	.extern	rts_error

act_cnt		=	-20					/* top of stack */
mask_arg	=	-16					/* r3 */
ret_val		=	-12					/* r2 */
lblidx		=	 -8					/* r1 */
rtnidx		=	 -4					/* r0 */
SAVE_SIZE	=	 24					/* Multiple of 8 to ensure stack alignment */

ENTRY op_mprofextexfun
	putframe						/* Save registers into current M stack frame */
	ldr	r4, [sp]					/* actual count of args on stack */
	push	{fp, lr}
	mov	r8, #0						/* We haven't done auto_zlink/auto_relink_check */
	mov	fp, sp						/* Copy stack pointer to r11 (fp) */
	sub	sp, sp, #SAVE_SIZE				/* establish space for locals - still 8 byte aligned */
	CHKSTKALIGN						/* Verify stack alignment */
	/*
	 * Note from here down, do *not* use REG_FRAME_POINTER which was overwritten above. REG_FRAME_POINTER  is an alias for
	 * register r11 (fp) which contains a copy of sp before sp was decremented by the save area size so r11 (fp) contains a
	 * pointer. Just past the save area we've allocated which is why all references are using negative offsets.
	 */
	str	r0, [fp, #rtnidx]				/* save incoming arguments */
	str	r1, [fp, #lblidx]
	str	r2, [fp, #ret_val]
	str	r3, [fp, #mask_arg]
	str	r4, [fp, #act_cnt]
	/*
	 * First up, check the label index to see if tiz negative. If so, we must use lnk_proxy as a base address
	 * and pseudo linkagetable. Else use the caller's linkage table.
	 */
	cmp	r1, #0						/* Use current frame linkage table or lnk_proxy? */
	bge	loadandgo
	/*
	 * We have a negative index. Use lnk_proxy as a proxy linkage table.
	 */
	ldr	r2, =gtm_threadgbl
	ldr	r2, [r2]					/* r2 contains threadgbl base */
	ldr	r3, =ggo_lnk_proxy
	add	r3, r2						/* -> lnk_proxy.rtnhdr_adr */
	cmp	r0, #0						/* Using proxy table, rtnhdr index must be 0 */
	bne	gtmcheck
	ldr	r0, [r3]					/* -> rtnhdr */
	cmp	r0, #0
	beq	gtmcheck					/* If rhdaddr == 0, not yet linked into image which */
								/* .. should never happen for indirects */
	cmp	r1, #-1						/* Using proxy table, label index must be -1 */
	bne	gtmcheck
	ldr	r1, [r3, #4]					/* ->label table code offset ptr */
	cmp	r1, #0
	beq	gtmcheck					/* If labaddr == 0 && rhdaddr != 0, label does not exist */
								/* .. which also should never happen for indirects */
	ldr	r12, [r3, #8]					/* See if a parameter list was supplied */
	cmp	r12, #0
	beq	fmllstmissing					/* If not, raise error */

	b	justgo						/* Bypass autorelink check for indirects (done by caller) */
	/*
	 * We have a non-negative index. Use args as indexes into caller's linkage table.
	 */
loadandgo:
	ldr	r3, [r5]					/* -> frame_pointer */
	ldr	r3, [r3, #msf_rvector_off]			/* -> frame_pointer->rvector (rtnhdr) */
	ldr	r3, [r3, #mrt_lnk_ptr]				/* -> frame_pointer->rvector->linkage_adr */
	lsl	r0, #2						/* arg * 4 = offset for rtnhdr ptr */
	add	r12, r0, r3
	ldr	r12, [r12]
	cmp	r12, #0						/* See if defined */
	beq	autozlink					/* No - try auto-zlink */
	mov	r0, r12						/* -> rtnhdr */
	/*
	 * Have rtnhdr to call now. If rtnhdr->zhist, we should do an autorelink check on this routine to see if it needs
	 * to be relinked. Only do this if r8 is 0 meaning we haven't already done an autorelink check or if we just
	 * loaded the routine via auto_zlink.
	 */
	cmp	r8, #0						/* Already checked/resolved? */
	bne	getlabeloff					/* Yes, bypass this check and resolve the label offset */
	ldr	r12, [r0, #mrt_zhist]				/* See if we need to do an autorelink check */
	cmp	r12, #0
	bne	autorelink_check				/* Need autorelink check */
getlabeloff:
	lsl	r1, #2						/* arg * 4 = offset for label offset ptr */
	add	r12, r3, r1
	ldr	r12, [r12]					/* See if defined */
	cmp	r12, #0
	beq	label_missing
	mov	r1, r12						/* -> label table code offset */
	ldr	r12, [r1, #4]
	cmp	r12, #0						/* If has_parms == 0, then issue an error */
	beq	fmllstmissing
	ldr	r1, [r1]
	/*
	 * Create stack frame and invoke routine
	 */
justgo:
	movs	r3, r1						/* &(code_offset) for this label (usually & of lntabent) */
	beq	label_missing
	ldr	r2, [r3]					/* Code offset for this label */
	ldr	r12, [r0, #mrt_ptext_adr]
	add	r2, r12						/* Transfer address: codebase reg + offset to label */
	ldr	r1, [r0, #mrt_lnk_ptr]				/* Linkage table address (context pointer) */
	bl	new_stack_frame_sp
	/*
	 * Move parameters into place
	 */
	ldr	r0, [fp, #act_cnt]				/* number of actuallist parms */
	ADJ_STACK_ALIGN_EVEN_ARGS r0
	movs	r12, r0						/* save count for push_parm call later */
	beq	done
	add	r1, fp, r12, LSL #2				/* where to copy from */
	add	r1, #8						/* don't forget the pushed fp at the beginning */
loop:
	ldr	r3, [r1], #-4
	push	{r3}
	subs	r12, #1
	bne	loop
done:
	push	{r0}						/* arg count */
	CHKSTKALIGN						/* Verify stack alignment */
	ldr	r3, [fp, #mask_arg]
	ldr	r2, [fp, #ret_val]				/* ret_value */
	ldr	r1, =dollar_truth
	ldr	r1, [r1]
	and	r1, #1
	add	r0, #4						/* include total count, $T, addr_ret_val, mask */
	bl	push_parm					/* push_parm(total_cnt, $T, ret_value, mask, argc [,arg1, arg2, ...]) */
retlab:
	mov	sp, fp						/* Unwind C stack back to caller (almost) */
	pop	{fp, r12}					/* Get fp (r11) saved at start (don't care about saved lr) */
	getframe						/* Load regs from top M frame (sets return address in lr) */
	mov	sp, fp						/* Now stack is completely unwound */
	bx	lr
/*
 * Drive auto_zlink to fetch module
 */
autozlink:
	cmp	r8, #0						/* Already did autorelink or autorelink check? */
	bne	gtmcheck
	ldr	r0, [fp, #rtnidx]				/* Get index arg back */
	bl	auto_zlink
	ldr	r0, [fp, #rtnidx]				/* Restore both args after call */
	ldr	r1, [fp, #lblidx]
	mov	r8, #1
	b	loadandgo
/*
 * Drive auto_relink_check to see if a newer routine should be loaded
 */
autorelink_check:
	cmp	r8, #0						/* Already did autorelink or autorelink check? */
	bne	gtmcheck
	ldr	r0, [fp, #rtnidx]				/* Restore both args as parms for call */
	ldr	r1, [fp, #lblidx]
	bl	auto_relink_check				/* r0 still populated by rtnhdr */
	ldr	r0, [fp, #rtnidx]				/* Restore both args after call */
	ldr	r1, [fp, #lblidx]
	mov	r8, #2
	b	loadandgo

/*
 * Raise GTMCHECK (pseudo-GTMASSERT since args are more difficult in assembler) when something really screwedup
 * occurs
 */
gtmcheck:
	ldr	r1, =#ERR_GTMCHECK
	mov	r0, #1
	bl	rts_error
	b	retlab
/*
 * Make call so we can raise the appropriate LABELMISSING error for the not-found label.
 */
label_missing:
	ldr	r0, [fp, #lblidx]				/* Index to linkage table and to linkage name table */
	bl	laberror
	b	retlab
/*
 * Raise missing formal list error
 */
fmllstmissing:
	ldr	r1, =#ERR_FMLLSTMISSING
	mov	r0, #1
	bl	rts_error
	b	retlab

	.end
