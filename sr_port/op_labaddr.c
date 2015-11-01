/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
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

#include "rtnhdr.h"
#include "zbreak.h"

USHBIN_ONLY(static LNR_TABENT *ind_lnr;)

USHBIN_ONLY(LNR_TABENT  **) NON_USHBIN_ONLY(LNR_TABENT *)op_labaddr(rhdtyp *routine, mval *label, int4 offset)
{
	rhdtyp		*real_routine, *routine_hdr;
	LNR_TABENT	*answer, *first_line;

	error_def(ERR_LABELMISSING);
	error_def(ERR_OFFSETINV);
	error_def(ERR_LABELONLY);

	MV_FORCE_STR(label);
#if defined (__alpha) && defined (__vms)
	if (PDSC_FLAGS == ((proc_desc *)routine)->flags) /* it's a procedure descriptor, not a routine header */
	{
		routine_hdr = (rhdtyp *)(((proc_desc *)routine)->code_address);
		/* Make sure this if-test is sufficient to distinguish between procedure descriptors and routine headers */
		assert(PDSC_FLAGS != ((proc_desc *)routine_hdr)->flags);
	} else
#endif
		routine_hdr = routine;
	if (routine_hdr->label_only && offset)
		rts_error(VARLSTCNT(4) ERR_LABELONLY, 2, mid_len(&routine_hdr->routine_name), routine_hdr->routine_name.c);
	answer = find_line_addr(routine_hdr, &label->str, 0);
	if (NULL == answer)
		rts_error(VARLSTCNT(4) ERR_LABELMISSING, 2, mid_len((mident *)label->str.addr), label->str.addr);
	real_routine = CURRENT_RHEAD_ADR(routine_hdr);
	first_line = LNRTAB_ADR(real_routine);
	answer += offset;
	if (answer < first_line || answer >= first_line + real_routine->lnrtab_len)
		rts_error(VARLSTCNT(5) ERR_OFFSETINV, 3, mid_len((mident *)label->str.addr), label->str.addr, offset);
	USHBIN_ONLY(
		ind_lnr = answer;
		return &ind_lnr;
		)
	NON_USHBIN_ONLY(return answer;)
}
