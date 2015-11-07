/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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
#ifdef UNIX
#include "anticipatory_freeze.h"
#endif

GBLREF	jnlpool_addrs		jnlpool;
GBLREF	gd_region		*gv_cur_region;
GBLREF	volatile boolean_t	in_wcs_recover;
GBLREF	int			process_exiting;
#ifdef UNIX
GBLREF	jnlpool_ctl_ptr_t	jnlpool_ctl;
#endif

error_def(ERR_JNLCLOSED);
error_def(ERR_REPLJNLCLOSED);

static	const	unsigned short	zero_fid[3];

uint4 jnl_file_lost(jnl_private_control *jpc, uint4 jnl_stat)
{	/* Notify operator and terminate journaling */
	unsigned int	status;
	sgmnt_addrs	*csa;
	seq_num		reg_seqno, jnlseqno;
	boolean_t	was_lockid = FALSE, instfreeze_environ;

	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	switch(jpc->region->dyn.addr->acc_meth)
	{
	case dba_mm:
	case dba_bg:
		csa = &FILE_INFO(jpc->region)->s_addrs;
		break;
	default:
		GTMASSERT;
	}
#	ifdef VMS
	/* The following assert has been removed as it could be FALSE if the caller is "jnl_file_extend"
	 *	assert(0 != memcmp(csa->nl->jnl_file.jnl_file_id.fid, zero_fid, SIZEOF(zero_fid)));
	 */
#	endif
	assert(csa->now_crit);
	/* We issue an rts_error (instead of shutting off journaling) in the following cases :					{BYPASSOK}
	 * 1) $gtm_error_on_jnl_file_lost is set to issue runtime error (if not already issued) in case of journaling issues.
	 * 2) The process has $gtm_custom_errors set (indicative of anticipatory freeze setup) in which case the goal is to
	 *    never shut-off journaling
	 * 3) If $gtm_custom_errors is not set for this process, but the source server was started with $gtm_custom_errors
	 *    set. This way, as long as the environment is configured for $gtm_custom_errors individual processes never turn
	 *    off journaling.
	 */
	UNIX_ONLY(instfreeze_environ = (ANTICIPATORY_FREEZE_AVAILABLE
						|| ((NULL != jnlpool_ctl) && jnlpool_ctl->instfreeze_environ_inited)));
	VMS_ONLY(instfreeze_environ = FALSE);
	if ((JNL_FILE_LOST_ERRORS == TREF(error_on_jnl_file_lost)) || instfreeze_environ)
	{
		VMS_ONLY(assert(FALSE)); /* Not fully implemented / supported on VMS. */
		if (!process_exiting || instfreeze_environ || !csa->jnl->error_reported)
		{
			csa->jnl->error_reported = TRUE;
			in_wcs_recover = FALSE;	/* in case we're called in wcs_recover() */
			if (SS_NORMAL != jpc->status)
				rts_error_csa(CSA_ARG(csa) VARLSTCNT(7) jnl_stat, 4, JNL_LEN_STR(csa->hdr),
						DB_LEN_STR(gv_cur_region), jpc->status);
			else
				rts_error_csa(CSA_ARG(csa) VARLSTCNT(6) jnl_stat, 4, JNL_LEN_STR(csa->hdr),
						DB_LEN_STR(gv_cur_region));
		}
		return jnl_stat;
	}
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
		send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_REPLJNLCLOSED, 6, DB_LEN_STR(jpc->region), &reg_seqno, &reg_seqno,
				&jnlseqno, &jnlseqno);
	} else
		send_msg_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_JNLCLOSED, 3, DB_LEN_STR(jpc->region), &csa->ti->curr_tn);
#ifdef VMS
	/* We can get a jnl_file_lost before the file is even created, so locking is done only if the lock exist */
	if (0 != csa->jnl->jnllsb->lockid)
	{
		was_lockid = TRUE;
		status = gtm_enqw(EFN$C_ENF, LCK$K_EXMODE, csa->jnl->jnllsb, LCK$M_CONVERT | LCK$M_NODLCKBLK,
				NULL, 0, NULL, 0, NULL, PSL$C_USER, 0);
		if (SS$_NORMAL == status)
			status = csa->jnl->jnllsb->cond;
	}
	jnl_file_close(jpc->region, FALSE, FALSE);
	if (was_lockid)
	{
		if (SS$_NORMAL == status)
			status = gtm_deq(csa->jnl->jnllsb->lockid, NULL, PSL$C_USER, 0);
		if (SS$_NORMAL != status)
			GTMASSERT;
	}
# else
	jnl_file_close(jpc->region, FALSE, FALSE);
#endif
	return EXIT_NRM;
}
