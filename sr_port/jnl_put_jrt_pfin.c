/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_string.h"
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

GBLREF 	jnl_gbls_t		jgbl;

void	jnl_put_jrt_pfin(sgmnt_addrs *csa)
{
	struct_jrec_pfin	pfin_record;

	assert(csa->now_crit);
	assert(0 != csa->jnl->pini_addr);
	pfin_record.prefix.jrec_type = JRT_PFIN;
	pfin_record.prefix.forwptr = pfin_record.suffix.backptr = PFIN_RECLEN;
	pfin_record.suffix.suffix_code = JNL_REC_SUFFIX_CODE;
	pfin_record.prefix.pini_addr  = (0 == csa->jnl->pini_addr) ? JNL_HDR_LEN : csa->jnl->pini_addr;
	pfin_record.prefix.tn = csa->ti->curr_tn;
	/* At this point jgbl.gbl_jrec_time should be set by the caller */
	assert(jgbl.gbl_jrec_time);
	if (!jgbl.gbl_jrec_time)
	{	/* no idea how this is possible, but just to be safe */
		JNL_SHORT_TIME(jgbl.gbl_jrec_time);
	}
	pfin_record.prefix.time = jgbl.gbl_jrec_time;
	jnl_write(csa->jnl, JRT_PFIN, (jnl_record *)&pfin_record, NULL, NULL);
}
