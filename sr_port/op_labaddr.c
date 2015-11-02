/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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

error_def(ERR_LABELMISSING);
error_def(ERR_LABELONLY);
error_def(ERR_OFFSETINV);

USHBIN_ONLY(lnr_tabent **) NON_USHBIN_ONLY(lnr_tabent *)op_labaddr(rhdtyp *routine, mval *label, int4 offset)
{
	rhdtyp		*real_routine, *routine_hdr;
	lnr_tabent	*answer, *first_line;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_STR(label);
#	if defined (__alpha) && defined (__vms)
	if (PDSC_FLAGS == ((proc_desc *)routine)->flags) /* it's a procedure descriptor, not a routine header */
	{
		routine_hdr = (rhdtyp *)(((proc_desc *)routine)->code_address);
		/* Make sure this if-test is sufficient to distinguish between procedure descriptors and routine headers */
		assert(PDSC_FLAGS != ((proc_desc *)routine_hdr)->flags);
	} else
#	endif
		routine_hdr = routine;
	if (!(routine_hdr->compiler_qlf & CQ_LINE_ENTRY) && (0 != offset))
		rts_error(VARLSTCNT(4) ERR_LABELONLY, 2, routine_hdr->routine_name.len, routine_hdr->routine_name.addr);
	answer = find_line_addr(routine_hdr, &label->str, 0, NULL);
	if (NULL == answer)
		rts_error(VARLSTCNT(4) ERR_LABELMISSING, 2, label->str.len, label->str.addr);
	real_routine = CURRENT_RHEAD_ADR(routine_hdr);
	first_line = LNRTAB_ADR(real_routine);
	answer += offset;
	if ((answer < first_line) || (answer >= (first_line + real_routine->lnrtab_len)))
		rts_error(VARLSTCNT(5) ERR_OFFSETINV, 3, label->str.len, label->str.addr, offset);
	/* Return the address for line number entry pointer/offset, so that the adjacent location in memory holds has_parms. */
	USHBIN_ONLY(
		(TREF(lab_proxy)).lnr_adr = answer;
		return &((TREF(lab_proxy)).lnr_adr);
	);
	NON_USHBIN_ONLY(
		/* On non-shared-binary, calculate the offset to the corresponding lnr_tabent record by subtracting
		 * the base address (routine header) from line number entry's address, and save the result in
		 * lab_ln_ptr field of lab_tabent structure.
		 */
		(TREF(lab_proxy)).lab_ln_ptr = (int4)answer - (int4)real_routine;
		return &((TREF(lab_proxy)).lab_ln_ptr);
	);
}
