/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_inet.h"
#ifdef VMS
#include <iodef.h>
#include <psldef.h>
#include <lckdef.h>
#include <efndef.h>
#include <descrip.h>
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
#include "repl_msg.h"
#include "gtmsource.h"

GBLREF	jnlpool_addrs		jnlpool;

static	const	unsigned short	zero_fid[3];

void jnl_file_lost(jnl_private_control *jpc, uint4 jnl_stat)
{	/* Notify operator and terminate journaling */
	unsigned int	status;
	sgmnt_addrs	*csa;
	seq_num		reg_seqno, jnlseqno;

	error_def(ERR_REPLJNLCLOSED);
	error_def(ERR_JNLCLOSED);

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
	/* The following assert has been removed as it could be FALSE if the caller is "jnl_file_extend"
	 *	assert(0 != memcmp(csa->nl->jnl_file.jnl_file_id.fid, zero_fid, sizeof(zero_fid)));
	 */
#endif
	assert(csa->now_crit);
	if (0 != jnl_stat)
		jnl_send_oper(jpc, jnl_stat);
	csa->hdr->jnl_state = jnl_closed;
	jpc->jnl_buff->cycle++; /* increment shared cycle so all future callers of jnl_ensure_open recognize journal switch */
	assert(jpc->cycle < jpc->jnl_buff->cycle);
	if (REPL_ENABLED(csa->hdr))
	{
		csa->hdr->repl_state = repl_was_open;
		reg_seqno = csa->hdr->reg_seqno;
		jnlseqno = (NULL != jnlpool.jnlpool_ctl) ? jnlpool.jnlpool_ctl->jnl_seqno : MAX_SEQNO;
		send_msg(VARLSTCNT(8) ERR_REPLJNLCLOSED, 6, DB_LEN_STR(jpc->region), &reg_seqno, &reg_seqno, &jnlseqno, &jnlseqno);
	} else
		send_msg(VARLSTCNT(5) ERR_JNLCLOSED, 3, DB_LEN_STR(jpc->region), &csa->ti->curr_tn);
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
