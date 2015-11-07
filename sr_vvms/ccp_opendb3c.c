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

#include <psldef.h>
#include <lckdef.h>
#include <efndef.h>


#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdsblk.h"
#include "ccp.h"
#include "jnl.h"
#include "locks.h"
#include "mlk_shr_init.h"

error_def(ERR_CCPJNLOPNERR);

/* AST routine entered on completion of sys$qio to update header in ccp_opendb3b */
void ccp_opendb3c(ccp_db_header *db)
{
	char		*c;
	sgmnt_addrs	*csa;
	sgmnt_data	*csd;
	unsigned int	i;
	uint4	status;

	assert(lib$ast_in_prog());

	csa = db->segment;
	csd = csa->hdr;

	if (JNL_ENABLED(csd))
		if (db->wm_iosb.valblk[CCP_VALBLK_JNL_ADDR] == 0)
		{
			db->wm_iosb.valblk[CCP_VALBLK_JNL_ADDR] = csa->jnl->jnl_buff->freeaddr;
			db->wm_iosb.valblk[CCP_VALBLK_EPOCH_TN] = csa->jnl->jnl_buff->epoch_tn;
			/* lastaddr is no longer a field in jnl_buff
			 *	db->wm_iosb.valblk[CCP_VALBLK_LST_ADDR] = csa->jnl->jnl_buff->lastaddr;
			 */
		}
		else
		{
			/* Open journal file, not first machine */
			jnl_file_open(db->greg, FALSE, ccp_closejnl_ast);
			if (csa->jnl->channel == 0)
			{
				ccp_close1(db);
				ccp_signal_cont(ERR_CCPJNLOPNERR);	/***** Is this reasonable? *****/
			}
			csa->jnl->jnl_buff->before_images = csd->ccp_jnl_before;
		}

	db->wm_iosb.valblk[CCP_VALBLK_TRANS_HIST] = csd->trans_hist.curr_tn + csd->trans_hist.lock_sequence;

	/* Convert Write-mode lock from Protected Write to Concurrent Read, writing the lock value block */
	status = ccp_enqw(EFN$C_ENF, LCK$K_CRMODE, &db->wm_iosb, LCK$M_CONVERT | LCK$M_VALBLK, NULL, 0,
			  NULL, 0, NULL, PSL$C_USER, 0);
	/***** Check error status here? *****/

	i = SIZEOF(sgmnt_data);
	assert((-(SIZEOF(int4) * 2) & i) == i);		/* check quadword alignment */
	csa->nl->bt_header_off = i;
	csa->nl->th_base_off = (i += csd->bt_buckets * SIZEOF(bt_rec));
	csa->nl->th_base_off += SIZEOF(que_ent);		/* Skip over links for hash table queue */
	csa->nl->bt_base_off = i + SIZEOF(bt_rec);		/* one unused rec to anchor TH queue */
	bt_init(csa);
	csa->nl->cache_off = -CACHE_CONTROL_SIZE(csd);
	db_csh_ini(csa);
	bt_que_refresh(db->greg);
	db_csh_ref(csa, TRUE);
	mlk_shr_init(csa->lock_addrs[0], csd->lock_space_size, csa, (FALSE == db->greg->read_only));
	db->greg->open = TRUE;

	/* Convert M-lock lock from Null to Concurrent Read, and establish a blocking AST routine */
	status = ccp_enq(0, LCK$K_CRMODE, &db->lock_iosb, LCK$M_CONVERT | LCK$M_SYNCSTS, NULL, 0,
			 ccp_lkrqwake1, db, ccp_lkdowake_blkast, PSL$C_USER, 0);
	/***** Check error status here? *****/

/*	csd->glob_sec_init = TRUE;	this field is not used by vms or the ccp */
	if (csd->staleness[0] != 0  ||  csd->staleness[1] != 0)		/* if values are 0, no stale timer */
		 db->stale_timer_id = db;

	ccp_request_write_mode(db);

	return;
}
