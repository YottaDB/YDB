/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <lckdef.h>
#include <psldef.h>
#include <ssdef.h>
#include <efndef.h>


#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "efn.h"
#include "error.h"
#include "jnl.h"
#include "locks.h"
#include "wcs_flu.h"

GBLREF	gd_region	*gv_cur_region;
GBLREF	boolean_t       is_src_server;
GBLREF 	jnl_gbls_t	jgbl;

static	const	int4    delta_30_sec[2] = { -300000000, -1 };

static	void	enq_timeout_ast(sgmnt_addrs *csa)
{
	uint4	status;

	assert(0 != csa->jnl->jnllsb->lockid);
	status = gtm_deq(csa->jnl->jnllsb->lockid, NULL, PSL$C_USER, LCK$M_CANCEL);
	assert(SS$_NORMAL == status);
	csa->jnl->jnllsb->lockid = 0;
}

uint4	set_jnl_file_close(set_jnl_file_close_opcode_t set_jnl_file_close_opcode)
{
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	uint4			status, jnl_status;
	jnl_private_control	*jpc;

	csa = &FILE_INFO(gv_cur_region)->s_addrs;
	csd = csa->hdr;
	jpc = csa->jnl;
	assert(!is_src_server);	/* source server does not hold the journal lock (jpc->jnllsb) so it should never come here */
	assert(TRUE == csa->now_crit);
	jnl_status = jnl_ensure_open();
	if (jnl_status)
		rts_error(VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(csd), DB_LEN_STR(gv_cur_region));
	assert(NOJNL != jpc->channel);
	status = sys$setimr(efn_timer, delta_30_sec, enq_timeout_ast, csa, 0);
	if (SS$_NORMAL == status)
	{	/* Notify any active GT.M processes (via a blocking AST) that they must close the journal file */
		csd->jnl_state = jnl_closed;
		status = gtm_enqw(EFN$C_ENF, LCK$K_EXMODE, jpc->jnllsb, LCK$M_CONVERT | LCK$M_NODLCKBLK,
						NULL, 0, NULL, 0, NULL, PSL$C_USER, 0);
		if (SS$_NORMAL == status)
			status = jpc->jnllsb->cond;
		if (SS$_NORMAL == status)
		{	/* The cantim below also has the side effect of cancelling all pending dbsync timers, but it is
			 * fine since anyway we are going to do the dbsync timer's job, i.e. a wcs_flu() right here.
			 */
			sys$cantim(csa, PSL$C_USER);
			if (0 != jpc->jnllsb->lockid)
			{
				status = gtm_deq(jpc->jnllsb->lockid, NULL, PSL$C_USER, 0);
				assert(SS$_NORMAL == status);
				jpc->jnllsb->lockid = 0;
			}
			/* Re-enable journalling so that we can clean up */
			csd->jnl_state = jnl_open;
			switch (set_jnl_file_close_opcode)
			{
			case SET_JNL_FILE_CLOSE_EXTEND:
				assert(0 == jpc->pini_addr);
				/* at this point we have the new journal opened but have released the jpc->jnllsb lock.
				 * the best thing to do here is to do a gtm_enqw() of jpc->jnllsb and get that journal lock
				 * in CR mode. but since this change is going in last minute in V4.3-001, and that change involves
				 * dealing with system calls, a relatively safer but sleazy overhead method is used.
				 * We close the file here. jnl_file_extend() then re-opens the journal immediately.
				 * jnl_ensure_open() makes sure we reopen the journal file and get the jnllsb lock in the
				 * appropriate mode. This can be improved in V4.4 --- nars -- 2002/04/18
				 */
				jnl_file_close(gv_cur_region, FALSE, FALSE);
				/* At this point, jpc->cycle == jb->cycle (due to the jnl_ensure_open() above in this routine)
				 * although jpc->channel is NOJNL. It is desired that jnl_ensure_open() will do only a
				 * JNL_FILE_SWITCHED(jpc) check (in the future) and not the (NOJNL == jpc->channel) check.
				 * That would mean the jnl_ensure_open() done immediately after returning from here to
				 * jnl_file_extend() is going to consider there is nothing to open as the cycle numbers match.
				 * To avoid this situation, we decrement the private cycle to induce a cycle mismatch.
				 */
				jpc->cycle--;
				break;
			case SET_JNL_FILE_CLOSE_RUNDOWN:
				if (!jgbl.mur_extract)
				{
					if (0 == jpc->pini_addr)
						jnl_put_jrt_pini(csa);
					jnl_put_jrt_pfin(csa);
				}
				if (dba_mm == csd->acc_meth)			/* this should parallel vvms/gds_rundown() */
					sys$cantim(gv_cur_region, PSL$C_USER);	/* see comment there for details. */
				jnl_file_close(gv_cur_region, TRUE, FALSE);
				break;
			case SET_JNL_FILE_CLOSE_SETJNL:
				assert(0 == jpc->pini_addr);
			case SET_JNL_FILE_CLOSE_BACKUP:
				if (0 == jpc->pini_addr)
					jnl_put_jrt_pini(csa);
				wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH);
				jnl_put_jrt_pfin(csa);
				jnl_file_close(gv_cur_region, TRUE, TRUE);
				break;
			default:
				GTMASSERT;
			}
		}
	}
	assert(SS$_NORMAL == status);
	return (status);
}
