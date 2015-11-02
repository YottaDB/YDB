/****************************************************************
 *								*
 *	Copyright 2012 Fidelity Information Services, Inc	*
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
#include "compiler.h"
#include "opcode.h"
#include "op.h"
#include "lv_val.h"
#include "mvalconv.h"
#include "glvn_pool.h"
#include "merge_def.h"
#include "filestruct.h"
#include "gdscc.h"
#include "copy.h"
#include "jnl.h"
#include "buddy_list.h"
#include "hashtab_int4.h"       /* needed for tp.h */
#include "tp.h"
#include "gvname_info.h"
#include "op_merge.h"

void op_indmerge2(uint4 indx)
{
	lv_val		*lv;
	opctype		oc;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (OC_SAVLVN == (oc = SLOT_OPCODE(indx)))	/* note assignment */
	{	/* lvn */
		lv = op_rfrshlvn(indx, OC_PUTINDX);
		op_merge_arg(MARG1_LCL, lv);
	} else if (OC_NOOP != oc)
	{	/* gvn */
		op_rfrshgvn(indx, oc);
		op_merge_arg(MARG1_GBL, NULL);
	}
	return;
}
