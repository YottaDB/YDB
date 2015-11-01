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

#ifdef VMS
#include <iodef.h>
#include <psldef.h>
#include <lckdef.h>
#include <efndef.h>
#endif

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "ccp.h"
#include "jnl.h"
#ifdef VMS
#include "locks.h"
#endif
#include "send_msg.h"


error_def(ERR_REPLJNLCLOSED);

static	const	unsigned short	zero_fid[3];

void jnl_file_lost(jnl_private_control *jpc, uint4 jnl_stat)
{	/* Notify operator and terminate journaling */
	unsigned int	status;
	sgmnt_addrs	*csa;

	switch(jpc->region->dyn.addr->acc_meth)
	{
	case dba_mm:
	case dba_bg:
		csa = &FILE_INFO(jpc->region)->s_addrs;
		break;
	default:
		GTMASSERT;
	}
#ifdef VMS
	assert(0 != memcmp(csa->hdr->jnl_file.jnl_file_id.fid, zero_fid, sizeof(zero_fid)));
#endif
	/* assert((TRUE == csa->now_crit) || ((TRUE == csa->hdr->clustered) && (CCST_CLOSED == csa->nl->ccp_state)));
	 * disabled for now because of the demon */
	if (0 != jnl_stat)
		jnl_send_oper(jpc, jnl_stat);
	csa->hdr->jnl_state = jnl_closed;
	if (REPL_ENABLED(csa->hdr))
	{
		csa->hdr->repl_state = repl_closed;
		send_msg(VARLSTCNT(4) ERR_REPLJNLCLOSED, 2, jpc->region->dyn.addr->fname_len,
			jpc->region->dyn.addr->fname);
	}
#ifdef VMS
	assert(0 != csa->jnl->jnllsb->lockid);
	status = gtm_enqw(EFN$C_ENF, LCK$K_EXMODE, csa->jnl->jnllsb, LCK$M_CONVERT | LCK$M_NODLCKBLK,
			NULL, 0, NULL, 0, NULL, PSL$C_USER, 0);
	if (SS$_NORMAL == status)
		status = csa->jnl->jnllsb->cond;
	jnl_file_close(jpc->region, FALSE, FALSE);
	if (SS$_NORMAL == status)
		status = gtm_deq(csa->jnl->jnllsb->lockid, NULL, PSL$C_USER, 0);
	if (SS$_NORMAL != status)
		GTMASSERT;
# else
	jnl_file_close(jpc->region, FALSE, FALSE);
#endif
}
