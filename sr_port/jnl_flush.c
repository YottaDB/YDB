/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "iosp.h"
#include "jnl.h"
#include "wbox_test_init.h"
#include "gtm_threadgbl.h"

GBLREF uint4 process_id;

uint4	jnl_flush(gd_region *reg)
{
	sgmnt_addrs		*csa;
 	node_local_ptr_t	cnl;
	jnl_private_control	*jpc;
	jnl_buffer_ptr_t	jb;
	uint4			status;
#	ifdef DEBUG
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	endif
	if (!reg || !reg->open)
		return SS_NORMAL;
	csa = &FILE_INFO(reg)->s_addrs;
	assert(csa->now_crit);
	jpc = csa->jnl;
	if (!JNL_ENABLED(csa->hdr) || (NULL == jpc) || (NOJNL == jpc->channel))
		return SS_NORMAL;
	jb = jpc->jnl_buff;
	jb->blocked = process_id;
	assert(jb->rsrv_freeaddr >= jb->freeaddr);
	assert((jb->rsrv_freeaddr >= jb->dskaddr)
		|| (gtm_white_box_test_case_enabled && (WBTEST_JNL_FILE_LOST_DSKADDR == gtm_white_box_test_case_number)));
	if (jb->rsrv_freeaddr != jb->dskaddr)
	{
		status = jnl_write_attempt(jpc, jb->rsrv_freeaddr);
		if (SS_NORMAL == status)
		{
			assert((jb->rsrv_freeaddr == jb->freeaddr)
				|| (gtm_white_box_test_case_enabled
					&& (WBTEST_JNL_FILE_LOST_DSKADDR == gtm_white_box_test_case_number)));
			cnl = csa->nl;
			INCR_GVSTATS_COUNTER(csa, cnl, n_jnl_flush, 1);
		}
	} else
		status = SS_NORMAL;
	assert(((SS_NORMAL == status) && (jb->dskaddr == jb->rsrv_freeaddr) && (jb->freeaddr == jb->rsrv_freeaddr))
		|| TREF(gtm_test_fake_enospc)
		|| (gtm_white_box_test_case_enabled && (WBTEST_JNL_FILE_LOST_DSKADDR == gtm_white_box_test_case_number)));
	jb->blocked = 0;
	return status;
}
