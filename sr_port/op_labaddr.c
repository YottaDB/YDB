/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#if defined (__alpha) && defined (__vms)
#include "pdscdef.h"
#include "proc_desc.h"
#endif

#include "cmd_qlf.h"
#include <rtnhdr.h>
#include "zbreak.h"
#include "linktrc.h"

error_def(ERR_LABELMISSING);
error_def(ERR_LABELONLY);
error_def(ERR_OFFSETINV);

/* Routine to return:
 *
 *    autorelink-enabled platform:	index into TABENT_PROXY containing label offset value (in bytes of generated code).
 *    non-autorelink-enabled platform:  address of label offset value (in bytes of generated code).
 *
 * Arguments:
 *
 *    routine - rtnhdr address
 *    label   - mval containing mval address
 *    offset  - integer value of offset as # of lines from label
 */
#ifdef USHBIN_SUPPORTED
#  ifdef AUTORELINK_SUPPORTED
int op_labaddr(int rtnidx, mval *label, int4 offset)
#  else
lnr_tabent **op_labaddr(rhdtyp *routine, mval *label, int4 offset)
#  endif
#else
lnr_tabent *op_labaddr(rhdtyp *routine, mval *label, int4 offset)
#endif
{
	rhdtyp		*real_routine, *routine_hdr;
	lnr_tabent	*answer, *first_line;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_STR(label);
	ARLINK_ONLY(assert(0 == rtnidx));		/* Should be 0 index for routine */
#	if defined (__alpha) && defined (__vms)
	if (PDSC_FLAGS == ((proc_desc *)routine)->flags) /* it's a procedure descriptor, not a routine header */
	{
		routine_hdr = (rhdtyp *)(((proc_desc *)routine)->code_address);
		/* Make sure this if-test is sufficient to distinguish between procedure descriptors and routine headers */
		assert(PDSC_FLAGS != ((proc_desc *)routine_hdr)->flags);
	} else
#	endif
		routine_hdr = ARLINK_ONLY(TADR(lnk_proxy)->rtnhdr_adr) NON_ARLINK_ONLY(routine);
	assert(NULL != routine_hdr);
	ARLINK_ONLY(DBGINDCOMP((stderr, "op_labaddr: Args: name: %.*s  rtnidx: %d  offset: %d\n",
				label->str.len, label->str.addr, rtnidx, offset)));
	NON_ARLINK_ONLY(DBGINDCOMP((stderr, "op_labaddr: Args: name: %.*s  rtnhdr: 0x"lvaddr"  offset: %d\n",
				    label->str.len, label->str.addr, routine_hdr, offset)));
	DBGINDCOMP((stderr, "op_labaddr: Routine containing label resolved to 0x"lvaddr"\n", routine_hdr));
	if (!(routine_hdr->compiler_qlf & CQ_LINE_ENTRY) && (0 != offset))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_LABELONLY, 2, routine_hdr->routine_name.len,
			      routine_hdr->routine_name.addr);
	answer = find_line_addr(routine_hdr, &label->str, 0, NULL);
	if (NULL == answer)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_LABELMISSING, 2, label->str.len, label->str.addr);
	real_routine = CURRENT_RHEAD_ADR(routine_hdr);
	first_line = LNRTAB_ADR(real_routine);
	answer += offset;
	DBGINDCOMP((stderr, "op_labaddr: label offset addr resolved to 0x"lvaddr"\n", answer));
	if ((answer < first_line) || (answer >= (first_line + real_routine->lnrtab_len)))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_OFFSETINV, 3, label->str.len, label->str.addr, offset);
	/* Return the index or address for line number entry pointer/offset, so that the adjacent location in memory holds
	 * has_parms. Note that if returning index for autorelink-enabled platform, the index is negative to indicate use
	 * of TABENT_PROXY.
	 */
#	ifdef USHBIN_SUPPORTED
	(TABENT_PROXY).lnr_adr = answer;
	return ARLINK_ONLY(-1) NON_ARLINK_ONLY(&((TREF(lab_proxy)).lnr_adr));
#	else
	/* On non-shared-binary, calculate the offset to the corresponding lnr_tabent record by subtracting
	 * the base address (routine header) from line number entry's address, and save the result in
	 * lab_ln_ptr field of TABENT_PROXY structure.
	 */
	(TABENT_PROXY).lab_ln_ptr = (int4)answer - (int4)real_routine;
	return &((TABENT_PROXY).lab_ln_ptr);
#	endif
}
