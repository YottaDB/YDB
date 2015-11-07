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

#include "gtm_string.h"

#include "cmd_qlf.h"
#include <rtnhdr.h>
#include "op.h"
#include "job_addr.h"
#include "zbreak.h"

GBLREF mident_fixed     zlink_mname;

error_def(ERR_JOBLABOFF);
error_def(ERR_ZLINKFILE);
error_def(ERR_ZLMODULE);

boolean_t job_addr(mstr *rtn, mstr *label, int4 offset, char **hdr, char **labaddr, boolean_t *need_rtnobj_shm_free)
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
		rt_hdr = find_rtn_hdr(rtn);
		if (NULL == rt_hdr)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_ZLINKFILE, 2, rtn->len, rtn->addr,
				      ERR_ZLMODULE, 2, STRLEN(&zlink_mname.c[0]), &zlink_mname);
		*need_rtnobj_shm_free = ARLINK_ONLY(rt_hdr->shared_object) NON_ARLINK_ONLY(FALSE);
		*hdr = (char *)rt_hdr;
	} else
		*need_rtnobj_shm_free = FALSE;
	lp = NULL;
	if ((rt_hdr->compiler_qlf & CQ_LINE_ENTRY) || (0 == offset))
		/* Label offset with routine compiled with NOLINE_ENTRY should cause error. */
		lp = find_line_addr(rt_hdr, label, offset, NULL);
	if (!lp)
		return (FALSE);
	/* Set the pointer to address / offset for line number entry storage in TABENT_PROXY. */
#	ifdef USHBIN_SUPPORTED
	ARLINK_ONLY((TABENT_PROXY).rtnhdr_adr = rt_hdr);
	(TABENT_PROXY).lnr_adr = lp;
#	else
	/* On non-shared-binary, calculcate the offset to the corresponding lnr_tabent record by subtracting
	 * the base address (routine header) from line number entry's address, and save the result in
	 * lab_ln_ptr field of TABENT_PROXY structure.
	 */
	(TABENT_PROXY).lab_ln_ptr = ((int4)lp - (int4)rt_hdr);
#	endif
	if (NULL != labaddr)
		*labaddr = (char *)LINE_NUMBER_ADDR(rt_hdr, lp);
	*hdr = (char *)rt_hdr;
	return (TRUE);
}
