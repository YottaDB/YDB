/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "cmd_qlf.h"
#include <rtnhdr.h>
#include "op.h"
#include "job_addr.h"
#include "zbreak.h"

error_def(ERR_JOBLABOFF);

boolean_t job_addr(mstr *rtn, mstr *label, int4 offset, char **hdr, char **labaddr)
{
	rhdtyp		*rt_hdr;
	int4		*lp;
	mval		rt;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (NULL == (rt_hdr = find_rtn_hdr(rtn)))
	{
		rt.mvtype = MV_STR;
		rt.str = *rtn;
		op_zlink(&rt, NULL);
		assertpro(NULL != (rt_hdr = find_rtn_hdr(rtn)));
	}
	lp = NULL;
	if ((rt_hdr->compiler_qlf & CQ_LINE_ENTRY) || (0 == offset))
		/* Label offset with routine compiled with NOLINE_ENTRY should cause error. */
		lp = find_line_addr(rt_hdr, label, offset, NULL);
	if (!lp)
		return (FALSE);
	/* Set the pointer to address / offset for line number entry storage in lab_proxy. */
	USHBIN_ONLY((TREF(lab_proxy)).lnr_adr = lp;)
	/* On non-shared-binary, calculcate the offset to the corresponding lnr_tabent record by subtracting
	 * the base address (routine header) from line number entry's address, and save the result in
	 * lab_ln_ptr field of lab_tabent structure.
	 */
	NON_USHBIN_ONLY((TREF(lab_proxy)).lab_ln_ptr = ((int4)lp - (int4)rt_hdr));
	if (NULL != labaddr)
		*labaddr = (char *)LINE_NUMBER_ADDR(rt_hdr, lp);
	*hdr = (char *)rt_hdr;
	return (TRUE);
}
