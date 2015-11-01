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
#include "jnl_write.h"
#include "repl_msg.h"
#include "gtmsource.h"

GBLREF  jnlpool_ctl_ptr_t	jnlpool_ctl;
GBLREF	inctn_opcode_t		inctn_opcode;
GBLREF 	jnl_gbls_t		jgbl;

void	jnl_write_inctn_rec(sgmnt_addrs	*csa)
{
	struct_jrec_inctn	inctn_record;

	assert(0 != csa->jnl->pini_addr);
	assert(csa->now_crit);
	assert((csa->ti->early_tn == csa->ti->curr_tn) || (csa->ti->early_tn == csa->ti->curr_tn + 1));
	inctn_record.prefix.jrec_type = JRT_INCTN;
	inctn_record.prefix.forwptr = inctn_record.suffix.backptr = INCTN_RECLEN;
	inctn_record.suffix.suffix_code = JNL_REC_SUFFIX_CODE;
	inctn_record.prefix.pini_addr = (0 == csa->jnl->pini_addr) ? JNL_HDR_LEN : csa->jnl->pini_addr;
	assert(jgbl.gbl_jrec_time);
	if (!jgbl.gbl_jrec_time)
	{	/* no idea how this is possible, but just to be safe */
		JNL_SHORT_TIME(jgbl.gbl_jrec_time);
	}
	inctn_record.prefix.time = jgbl.gbl_jrec_time;
	inctn_record.prefix.tn = csa->ti->curr_tn;
	assert(inctn_opcode_total > inctn_opcode && inctn_invalid_op < inctn_opcode);
	inctn_record.opcode = inctn_opcode;
	jnl_write(csa->jnl, JRT_INCTN, (jnl_record *)&inctn_record, NULL, NULL);
}
