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
#include "gtm_time.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "jnl_write.h"

GBLREF	jnl_process_vector	*prc_vec;

void	jnl_put_jrt_pfin(sgmnt_addrs *csa)
{
	struct_jrec_pfin	pfin_record;

	assert(csa->now_crit);
	assert(csa->jnl->pini_addr != 0);
	memcpy(&pfin_record.process_vector, prc_vec, sizeof(jnl_process_vector));
	JNL_WHOLE_TIME(pfin_record.process_vector.jpv_time);
	pfin_record.tn = csa->hdr->trans_hist.curr_tn;
	pfin_record.pini_addr = csa->jnl->pini_addr;
	jnl_write(csa->jnl, JRT_PFIN, (jrec_union *)&pfin_record, NULL, NULL);
}
