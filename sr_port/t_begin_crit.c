/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
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
#include "gdsblk.h"
#include "gdscc.h"
#include "filestruct.h"
#include "jnl.h"
#include "t_begin_crit.h"
#include "cws_insert.h"

GBLDEF	srch_hist		dummy_hist;

GBLREF	gd_region		*gv_cur_region;
GBLREF	jnl_format_buffer	*non_tp_jfb_ptr;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	trans_num		start_tn;
GBLREF	uint4			t_err;
GBLREF	unsigned char		cw_set_depth;
GBLREF	unsigned int		t_tries;

void t_begin_crit(uint4 err)
/* err - error code for current gvcst_routine */
{
	cws_reset();
	start_tn = cs_addrs->ti->curr_tn;
	cw_set_depth = 0;
	t_tries = CDB_STAGNATE;
	t_err = err;
	if (non_tp_jfb_ptr)
		non_tp_jfb_ptr->record_size = 0; /* re-initialize it to 0 since TOTAL_NONTPJNL_REC_SIZE macro uses it */
	grab_crit(gv_cur_region);
}
