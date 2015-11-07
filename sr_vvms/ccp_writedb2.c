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
#include <fab.h>
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "ccp.h"
#include "jnl.h"
#include <iodef.h>
#include <ssdef.h>
#include "wcs_recover.h"
#include "ccp_writedb2.h"
#include "ccp_writedb3.h"


error_def(ERR_CCPSIGDMP);

GBLREF	uint4	process_id;



/* AST routine entered on completion of sys$qio to read transaction history in ccp_reqwm_interrupt */

void ccp_writedb2(ccp_db_header *db)
{
	bt_rec		*btr, *que_base, *que_top;
	cache_rec	*wtop, *w;
	sgmnt_addrs	*cs_addrs;
	int		i;
	uint4	status;


	assert(lib$ast_in_prog());

	cs_addrs = db->segment;
	if (cs_addrs == NULL  ||  cs_addrs->nl->ccp_state == CCST_CLOSED)
		return;

	if ((db->qio_iosb.cond & 1) == 0)
		ccp_signal_cont(db->qio_iosb.cond);	/***** Is this reasonable? *****/

	if (JNL_ENABLED(cs_addrs->hdr))
		cs_addrs->jnl->jnl_buff->filesize = cs_addrs->ti->ccp_jnl_filesize;

/******************************************************************************

There are several possible threads of execution subsequent to the completion of
this routine.  If the master map needs to be re-read, then we inititate the qio
read.   The completions ast for this read, ccp_writedb4, checks the status and
then either initiates a read to update the lock section if necessary or calls
ccp_writedb5 which posts an action request to finish the job.  On the  other
hand, if the master map is up to date, then we either read the lock section if
necessary or call ccp_writedb5 at this time so that the next step will be
effected upon completion of this routine.  The completion ast for the lock read
checks the status and then  calls ccp_writedb5 to post an action request to
finish the job. We post the reads first in order to overlap the I/O and
processing. (Note: further improvement could be obtained by not waiting on
completion of the master map read until some time later when a GT.M process
required the master map.  Would still need to update the total_blks field.)

******************************************************************************/

	assert (cs_addrs->ti->curr_tn == cs_addrs->ti->early_tn);

	if (db->wm_iosb.valblk[CCP_VALBLK_TRANS_HIST] == 0)
	{
		status = sys$qio(0, FILE_INFO(db->greg)->fab->fab$l_stv, IO$_READVBLK, &db->qio_iosb, ccp_writedb3, db,
				 cs_addrs->db_addrs[0], (MM_BLOCK - 1) * 512, 1, 0, 0, 0);
		if (status == SS$_IVCHAN)
			/* database has been closed out, section deleted */
			return;
		if (status != SS$_NORMAL)
			ccp_signal_cont(status);	/***** Is this reasonable? *****/
	}
	else
		if (db->master_map_start_tn < cs_addrs->ti->mm_tn)
		{
			status = sys$qio(0, FILE_INFO(db->greg)->fab->fab$l_stv, IO$_READVBLK, &db->qio_iosb, ccp_writedb4, db,
					 MM_ADDR(db->glob_sec), MASTER_MAP_SIZE(db->glob_sec), MM_BLOCK, 0, 0, 0);
			if (status == SS$_IVCHAN)
				/* database has been closed out, section deleted */
				return;
			if (status != SS$_NORMAL)
				ccp_signal_cont(status);	/***** Is this reasonable? *****/
			db->master_map_start_tn = cs_addrs->ti->mm_tn;
		}
		else
			if (db->last_lk_sequence < cs_addrs->ti->lock_sequence)
			{
				status = sys$qio(0, FILE_INFO(db->greg)->fab->fab$l_stv, IO$_READVBLK, &db->qio_iosb,
						 ccp_writedb4a, db, cs_addrs->lock_addrs[0], db->glob_sec->lock_space_size,
						 LOCK_BLOCK(db->glob_sec) + 1, 0, 0, 0);
				if (status == SS$_IVCHAN)
					/* database has been closed out, section deleted */
					return;
				if (status != SS$_NORMAL)
					ccp_signal_cont(status);	/***** Is this reasonable? *****/
				db->last_lk_sequence = cs_addrs->ti->lock_sequence;
			}
			else
				ccp_writedb5(db);

	w = &cs_addrs->acc_meth.bg.cache_state->cache_array;
	w += cs_addrs->hdr->bt_buckets;
	for (wtop = w + cs_addrs->hdr->n_bts;  w < wtop;  ++w)
	{
		if (w->blk != CR_BLKEMPTY)
			if ((btr = ccp_bt_get(cs_addrs, w->blk)) == NULL)
			{
				if (w->tn <= OLDEST_HIST_TN(cs_addrs))
				{
					w->cycle++;	/* increment cycle whenever blk number changes (tp_hist depends on this) */
					w->blk = CR_BLKEMPTY;
				}
				w->bt_index = 0;
			}
			else
				if (btr->tn < db->last_write_tn)	/* not changed since dropped write mode */
				{
					w->bt_index = GDS_ABS2REL(btr);
					btr->cache_index = GDS_ABS2REL(w);
				}
				else
				{
					assert(0 == w->dirty);
					btr->cache_index = CR_NOTVALID;
					w->cycle++;	/* increment cycle whenever blk number changes (tp_hist depends on this) */
					w->blk = CR_BLKEMPTY;
					w->bt_index = 0;
				}
		assert(0 == w->dirty);
	}

	for (;;)
	{
		for (que_base = cs_addrs->bt_header, que_top = que_base + cs_addrs->hdr->bt_buckets;
		     que_base < que_top;
		     ++que_base)
		{
			assert(que_base->blk == BT_QUEHEAD);
			i = 0;
			for (btr = (bt_rec *)((char *)que_base + que_base->blkque.fl);
			     btr != que_base;
			     btr = (bt_rec *)((char *)btr + btr->blkque.fl))
			{
				if (btr->cache_index != CR_NOTVALID)
				{
					w = GDS_REL2ABS(btr->cache_index);
					if (w->blk != btr->blk)
						btr->cache_index = CR_NOTVALID;
				}

				if (i > cs_addrs->hdr->n_bts)
					break;
				++i;
			}

			if (i > cs_addrs->hdr->n_bts)
				break;
		}

		if (i > cs_addrs->hdr->n_bts)
		{
			lib$signal(ERR_CCPSIGDMP, 1);
			wcs_recover(db->greg);
		}
		else
			break;
	}

	if (cs_addrs->now_crit)
	{
		assert(cs_addrs->nl->in_crit == process_id);
		cs_addrs->nl->in_crit = 0;
		(void)mutex_unlockw(cs_addrs->critical, cs_addrs->critical->crashcnt, &cs_addrs->now_crit);
		/***** Check error status here? *****/
	}
}
