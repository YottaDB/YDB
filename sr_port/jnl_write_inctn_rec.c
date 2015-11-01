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

#include <netinet/in.h> /* Required for gtmsource.h */
#include <arpa/inet.h>
#ifdef VMS
#include <descrip.h> /* Required for gtmsource.h */
#endif

#include "gtm_time.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "jnl_write.h"

GBLREF  jnlpool_ctl_ptr_t	jnlpool_ctl;
GBLREF	inctn_opcode_t		inctn_opcode;
GBLREF	uint4			cur_logirec_short_time;	/* see comment in gbldefs.c for usage */
GBLREF 	boolean_t		forw_phase_recovery;

void	jnl_write_inctn_rec(sgmnt_addrs	*csa)
{
	struct_jrec_inctn	inctn_record;

	inctn_record.pini_addr = csa->jnl->pini_addr;
	if (!forw_phase_recovery)
	{
		JNL_SHORT_TIME(inctn_record.short_time);
	} else
		inctn_record.short_time = cur_logirec_short_time;
	inctn_record.tn = csa->ti->curr_tn;
	assert(inctn_invalid_op > inctn_opcode && 0 <= inctn_opcode);
	inctn_record.opcode = inctn_opcode;
	jnl_write(csa->jnl, JRT_INCTN, (jrec_union *)&inctn_record, NULL, NULL);
}
