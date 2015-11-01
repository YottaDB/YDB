/****************************************************************
 *								*
 *	Copyright 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "longset.h"		/* needed for cws_insert.h */
#include "hashtab_int4.h"	/* needed for cws_insert.h */
#include "cws_insert.h"		/* for CWS_RESET macro */
#include "t_abort.h"		/* for prototype of t_abort() */

void t_abort(gd_region *reg, sgmnt_addrs *csa)
{
	assert(&FILE_INFO(reg)->s_addrs == csa);
	CWS_RESET;
	if (csa->now_crit)
		rel_crit(reg);
}
