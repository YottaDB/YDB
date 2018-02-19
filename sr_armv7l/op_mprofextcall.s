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

/* op_mprofextcall.s */

/*
	op_extcall calls an external GT.M MUMPS routine.  If the routine has
	not yet been linked into the current image, op_extcall will first link
	it by invoking the auto-ZLINK function.

	Args:
		r0 - routine hdr addr - address of procedure descriptor of routine to call
		r1 - label addr	- address of offset into routine to transfer control
*/

/*
 * op_extcall calls an external GT.M MUMPS routine with no arguments. If the routine
 * has not yet been linked into the current image, op_extcall will first link it by
 * invoking the auto-ZLINK function. Before driving the function, we check if *any*
 * routines have been linked in as autorelink-enabled or if any directories are
 * autorelink-enabled and if so, drive a check to see if a newer version exists that
 * could be linked in.
 *
 * Parameters:
 *
 *  - arg0 (r0) - Index into linkage table of caller containing the routine header
 *		  address of the routine to call.
 *  - arg1 (r1) - Index into linkage table of caller containing the address of an
 *		  offset into routine to which to transfer control associated with
 *		  a given label. This value is typically the address of the lnr_adr
 *		  field in a label entry
 *
 * Note if arg1 is negative, this means the linkage table to use is not from the
 * caller but is contained in TREF(lnk_proxy) used by indirects and other dynamic
 * code (like callins).
 *
 * Note the return address is also supplied (on the stack) but we remove that immediately
 * since we do not return directly to it but to the the called rtn when the return address
 * is loaded out of the top M stackframe by getframe.
 *
 * Note we use r8 as a flag that we don't do more than one of auto_zlink() OR auto_relink_check()
 * call. This keeps us out of any possible loop condition as only one or the other should
 * ever be necessary. Register r8 is also known as REG_LITERAL_BASE and is saved by the putframe
 * macro so we need not save it separately.
 */

	.title	op_mprofextcall.s

	.include "linkage.si"
	.include "g_msf.si"
	.include "gtm_threadgbl_deftypes_asm.si"
	.include "stack.si"
#	include "debug.si"

	.sbttl	op_mprofextcall

	.data
	.extern	ERR_LABELUNKNOWN
	.extern	frame_pointer

	.text
	.extern	auto_zlink
	.extern auto_relink_check
	.extern	new_stack_frame_sp
	.extern	rts_error
	.extern laberror

/*
 * Define offsets for arguments saved in stack space
 */
stack_arg1	= -8
stack_arg0	= -4
SAVE_SIZE	=  8

ENTRY op_mprofextcall
	push	{fp, r12}					/* r12 is to maintain 8 byte stack alignment */
	putframe
	mov	fp, sp
	sub	sp, #SAVE_SIZE					/* Establish space for saving arguments - still 8 byte aligned */
	CHKSTKALIGN						/* Verify stack alignment */
	mov	r8, #0						/* Init flag - We haven't done auto_zlink/auto_relink_check */
	str	r0, [fp, #stack_arg0]				/* Save index args */
	str	r1, [fp, #stack_arg1]
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
	b	justgo						/* Bypass autorelink check for indirects (done by caller) */
	/*
	 * We have a non-negative index. Use args as indexes into caller's linkage table.
	 */
loadandgo:
	ldr	r12, [r5]					/* -> frame_pointer */
	ldr	r3, [r12, #msf_rvector_off]			/* -> frame_pointer->rvector (rtnhdr) */
	ldr	r3, [r3, #mrt_lnk_ptr]				/* -> frame_pointer->rvector->linkage_adr */
	lsl	r0, #2						/* arg * 4 = offset for rtnhdr ptr */
	add	r2, r0, r3
	ldr	r2, [r2]
	cmp	r2, #0						/* See if defined */
	beq	autozlink					/* No - try auto-zlink */
	mov	r0, r2						/* -> rtnhdr */
	/*
	 * Have rtnhdr to call now. If rtnhdr->zhist, we should do an autorelink check on this routine to see if it needs
	 * to be relinked. Only do this if r8 is 0 meaning we haven't already done an autorelink check or if we just
	 * loaded the routine via auto_zlink.
	 */
	cmp	r8, #0						/* Already checked/resolved? */
	bne	getlabeloff					/* Yes, bypass this check and resolve the label offset */
	ldr	r4, [r0, #mrt_zhist]				/* See if we need to do an autorelink check */
	cmp	r4, #0
	bne	autorelink_check				/* Need autorelink check */
getlabeloff:
	lsl	r1, #2						/* arg * 4 = offset for label offset ptr */
	add	r2, r1, r3
	ldr	r2, [r2]					/* See if defined */
	cmp	r2, #0
	beq	label_missing
	ldr	r1, [r2]					/* -> label table code offset */
	/*
	 * Create stack frame and invoke routine
	 */
justgo:
	movs	r4, r1						/* &(code_offset) for this label (usually & of lntabent) */
	beq	label_missing
	ldr	r2, [r4]					/* Code offset for this label */
	ldr	r1, [r0, #mrt_ptext_adr]
	add	r2, r1						/* Transfer address: codebase reg + offset to label */
	ldr	r1, [r0, #mrt_lnk_ptr]				/* Linkage table address (context pointer) */
	bl	new_stack_frame_sp
retlab:								/* If error, return to caller, else "return" to callee */
	getframe						/* Sets regs (including r8) as they should be for new frame */
	mov	sp, fp
	pop	{fp, r12}
	bx	lr
/*
 * Drive auto_zlink to fetch module
 */
autozlink:
	cmp	r8, #0						/* Already did autorelink or autorelink check? */
	bne	gtmcheck
	ldr	r0, [fp, #stack_arg0]				/* Get index arg back */
	bl	auto_zlink
	ldr	r0, [fp, #stack_arg0]				/* Restore both args after call */
	ldr	r1, [fp, #stack_arg1]
	mov	r8, #1
	b	loadandgo
/*
 * Drive auto_relink_check to see if a newer routine should be loaded
 */
autorelink_check:
	cmp	r8, #0						/* Already did autorelink or autorelink check? */
	bne	gtmcheck
	ldr	r0, [fp, #stack_arg0]				/* Restore both args as parms for call */
	ldr	r1, [fp, #stack_arg1]
	bl	auto_relink_check
	ldr	r0, [fp, #stack_arg0]				/* Restore both args after call */
	ldr	r1, [fp, #stack_arg1]
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
	ldr	r0, [fp, #stack_arg1]				/* Index to linkage table and to linkage name table */
	bl	laberror
	b	retlab

	.end
