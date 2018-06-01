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

/* op_extcall.s */

/*
	op_extcall calls an external GT.M MUMPS routine.  If the routine has
	not yet been linked into the current image, op_extcall will first link
	it by invoking the auto-ZLINK function.

	Args:
		x0 - routine hdr addr - address of procedure descriptor of routine to call
		w1 - index to offset into routine to transfer control
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
 *  - arg0 (x0) - Index into linkage table of caller containing the routine header
 *		  address of the routine to call.
 *  - arg1 (w1) - Index into linkage table of caller containing the address of an
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
	.extern	frame_pointer
	.extern gtm_threadgbl

	.text
	.extern	auto_zlink
	.extern auto_relink_check
	.extern	new_stack_frame
	.extern	rts_error
	.extern laberror

/*
 * Define offsets for arguments saved in stack space
 */
	
ENTRY op_extcall
	putframe
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	CHKSTKALIGN						/* Verify stack alignment */
	mov	w22, wzr					/* Init flag - We haven't done auto_zlink/auto_relink_check */
	sxtw	x1, w1						/* Sign extend arg1 */
	mov	x28, x1						/* Save index args */
	mov	x27, x0
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
	ldr	x1, [x3, #8]					/* ->label table code offset ptr */
	cbz	x1, gtmcheck					/* If labaddr == 0 && rhdaddr != 0, label does not exist */
								/* .. which also should never happen for indirects */
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
	cbnz	w22, getlabeloff				/* Already checked-bypass this check and resolve the label offset */
	ldr	x4, [x0, #mrt_zhist]				/* See if we need to do an autorelink check */
	cbnz	x4, autorelink_check				/* Need autorelink check */
getlabeloff:
	lsl	x1, x1, #3					/* arg * 8 = offset for label offset ptr */
	add	x2, x1, x3
	ldr	x2, [x2]					/* See if defined */
	cbz	x2, label_missing
	ldr	x1, [x2]					/* -> label table code offset */
	/*
	 * Create stack frame and invoke routine
	 */
justgo:
	cbz	x1, label_missing
	ldr	w2, [x1]					/* Code offset for this label */
	ldr	x1, [x0, #mrt_ptext_adr]
	add	x2, x1, w2, sxtw				/* Transfer address: codebase reg + offset to label */
	ldr	x1, [x0, #mrt_lnk_ptr]				/* Linkage table address (context pointer) */
	bl	new_stack_frame
retlab:								/* If error, return to caller, else "return" to callee */
	getframe						/* Sets regs (including w22) as they should be for new frame */
	mov	sp, x29
	ldp	x29, x15, [sp], #16				/* Want the return address from the getframe, not the stack one */
	ret
/*
 * Drive auto_zlink to fetch module
 */
autozlink:
	cbnz	w22, gtmcheck					/* Already did autorelink or autorelink check? */
	mov	x0, x27						/* Get index arg back */
	bl	auto_zlink
	mov	x0, x27						/* Restore both args after call */
	mov	x1, x28
	mov	w22, #1
	b	loadandgo
/*
 * Drive auto_relink_check to see if a newer routine should be loaded
 */
autorelink_check:
	cbnz	w22, gtmcheck					/* Already did autorelink or autorelink check? */
	mov	x1, x28						/* Restore both args as parms for call */
	mov	x0, x27
	bl	auto_relink_check
	mov	x1, x28						/* Restore both args after call */
	mov	x0, x27
	mov	w22, #2
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
