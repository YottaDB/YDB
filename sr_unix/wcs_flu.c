/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <sys/mman.h>
#include <sys/shm.h>

#include "gtm_string.h"
#include "gtm_time.h"
#include "gtm_unistd.h"	/* fsync() needs this */

#include "aswp.h"	/* for ASWP */
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "iosp.h"		/* for SS_NORMAL */
#include "gdsbgtr.h"
#include "jnl.h"
#include "lockconst.h"		/* for LOCK_AVAILABLE */
#include "interlock.h"
#include "sleep_cnt.h"
#include "performcaslatchcheck.h"
#include "send_msg.h"
#include "gt_timer.h"
#include "is_file_identical.h"
#include "gtmmsg.h"
#include "wcs_sleep.h"
#include "wcs_flu.h"
#include "wcs_recover.h"

GBLREF	gd_region	*gv_cur_region;
GBLREF	uint4		process_id;
GBLREF	sgmnt_addrs	*cs_addrs;
GBLREF	volatile int4	db_fsync_in_prog;	/* for DB_FSYNC macro usage */
GBLREF 	jnl_gbls_t	jgbl;
GBLREF 	bool		in_backup;

#define	WAIT_FOR_CONCURRENT_WRITERS_TO_FINISH(fix_in_wtstart)							\
if (cnl->in_wtstart)												\
{														\
	DEBUG_ONLY(int4	in_wtstart;) 	/* temporary for debugging purposes */					\
	error_def(ERR_WRITERSTUCK);										\
														\
	assert(csa->now_crit);											\
	csd->wc_blocked = TRUE;		/* to stop all active writers */					\
	lcnt = 0;												\
	do													\
	{													\
		DEBUG_ONLY(in_wtstart = cnl->in_wtstart;)							\
		if (MAXGETSPACEWAIT == ++lcnt)									\
		{												\
			assert(FALSE);										\
			cnl->wcsflu_pid = 0;									\
			csd->wc_blocked = FALSE;								\
			if (!was_crit)										\
				rel_crit(gv_cur_region);							\
			send_msg(VARLSTCNT(5) ERR_WRITERSTUCK, 3, cnl->in_wtstart, DB_LEN_STR(gv_cur_region));	\
			return FALSE;										\
		}												\
		if (-1 == shmctl(udi->shmid, IPC_STAT, &shm_buf))						\
		{												\
			save_errno = errno;									\
			if (1 == lcnt)										\
			{											\
                		send_msg(VARLSTCNT(4) ERR_DBFILERR, 2, DB_LEN_STR(gv_cur_region));		\
				send_msg(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("shmctl()"), CALLFROM, save_errno);\
			} 											\
		} else if (1 == shm_buf.shm_nattch)								\
		{												\
			assert(FALSE == csa->in_wtstart  &&  0 <= cnl->in_wtstart);				\
			cnl->in_wtstart = 0;	/* fix improper value of in_wtstart if you are standalone */	\
			fix_in_wtstart = TRUE;									\
		} else												\
			wcs_sleep(lcnt);		/* wait for any in wcs_wtstart to finish */		\
	} while (0 < cnl->in_wtstart);										\
	csd->wc_blocked = FALSE;										\
}

bool wcs_flu(bool options)
{
	bool			ret = TRUE, success, was_crit;
	boolean_t		fix_in_wtstart, flush_hdr, jnl_enabled, sync_epoch, write_epoch, need_db_fsync;
	unsigned int		lcnt, pass;
	int			save_errno, wtstart_errno;
	jnl_buffer_ptr_t	jb;
	jnl_private_control	*jpc;
	uint4			jnl_status, to_wait, to_msg;
        unix_db_info    	*udi;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	node_local_ptr_t	cnl;
	file_control		*fc;
	cache_que_head_ptr_t	crq;
        struct shmid_ds         shm_buf;

	error_def(ERR_JNLFILOPN);
	error_def(ERR_WAITDSKSPACE);
	error_def(ERR_OUTOFSPACE);
	error_def(ERR_WCBLOCKED);
	error_def(ERR_SYSCALL);
	error_def(ERR_DBFILERR);


	jnl_status = 0;
	flush_hdr = options & WCSFLU_FLUSH_HDR;
	write_epoch = options & WCSFLU_WRITE_EPOCH;
	sync_epoch = options & WCSFLU_SYNC_EPOCH;
	need_db_fsync = options & WCSFLU_FSYNC_DB;
	udi = FILE_INFO(gv_cur_region);
	csa = &udi->s_addrs;
	csd = csa->hdr;
	cnl = csa->nl;
	BG_TRACE_ANY(csa, total_buffer_flush);

	if (!(was_crit = csa->now_crit))	/* Caution: assignment */
		grab_crit(gv_cur_region);
	cnl->wcsflu_pid = process_id;
	if (dba_mm == csd->acc_meth)
	{
		ret = TRUE;
#if defined(UNTARGETED_MSYNC)
		if (csa->ti->last_mm_sync != csa->ti->curr_tn)
		{
			if (0 == msync((caddr_t)csa->db_addrs[0], (size_t)(csa->db_addrs[1] - csa->db_addrs[0]), MS_ASYNC))
				csa->ti->last_mm_sync = csa->ti->curr_tn;	/* Save when did last full sync */
			else
				ret = FALSE;
		}
#else
		csd->wc_blocked = TRUE;		/* to stop all active writers */
		for (lcnt = 1; (0 < cnl->in_wtstart  &&  MAXGETSPACEWAIT > lcnt); lcnt++)
			wcs_sleep(lcnt);		/* wait for any in wcs_wtstart to finish */
		if (MAXGETSPACEWAIT == lcnt)
		{
			assert(FALSE);
			cnl->wcsflu_pid = 0;
			if (!was_crit)
				rel_crit(gv_cur_region);
			return FALSE;
		}
		csd->wc_blocked = FALSE;
		while (0 != csa->acc_meth.mm.mmblk_state->mmblkq_active.fl)
			wcs_wtstart(gv_cur_region, csd->n_bts);
		fileheader_sync(gv_cur_region);
#endif
		cnl->wcsflu_pid = 0;
		if (!was_crit)
			rel_crit(gv_cur_region);
		return ret;
	}
	/* jnl_enabled is an overloaded variable. It is TRUE only if JNL_ENABLED(csd) is TRUE
	 * and if the journal file has been opened in shared memory. If the journal file hasn't
	 * been opened in shared memory, we needn't (and shouldn't) do any journal file activity.
	 */
	jnl_enabled = (JNL_ENABLED(csd) && (0 != cnl->jnl_file.u.inode));
	if (jnl_enabled)
	{
		jpc = csa->jnl;
		jb = jpc->jnl_buff;
		assert(csa == cs_addrs);	/* for jnl_ensure_open */
		jnl_status = jnl_ensure_open();
		if (SS_NORMAL != jnl_status)
		{
			assert(ERR_JNLFILOPN == jnl_status);
			cnl->wcsflu_pid = 0;
			if (!was_crit)
				rel_crit(gv_cur_region);
			send_msg(VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(csd), DB_LEN_STR(gv_cur_region));
			return FALSE;
		}
		if (jb->fsync_dskaddr != jb->freeaddr)
		{
			assert(jb->fsync_dskaddr <= jb->dskaddr);
			jnl_flush(gv_cur_region);
			assert(jb->freeaddr == jb->dskaddr);
			jnl_fsync(gv_cur_region, jb->dskaddr);
			assert(jb->fsync_dskaddr == jb->dskaddr);
		}
	}

	/* Note that calling wcs_wtstart just once assumes that if we ask it to flush all the buffers, it will.
	 * This may not be true in case of twins. But this is Unix. So not an issue.
	 */
	wtstart_errno = wcs_wtstart(gv_cur_region, csd->n_bts);		/* Flush it all */
	/* At this point the cache should have been flushed except if some other process is in wcs_wtstart waiting
	 * to flush the dirty buffer that it has already removed from the active queue. Wait for it to finish.
	 */
	fix_in_wtstart = FALSE;		/* set to TRUE by the following macro if we needed to correct cnl->in_wtstart */
	WAIT_FOR_CONCURRENT_WRITERS_TO_FINISH(fix_in_wtstart);
	/* Ideally at this point, the cache should have been flushed. But there is a possibility that the other
	 *   process in wcs_wtstart which had already removed the dirty buffer from the active queue found (because
	 *   csr->jnl_addr > jb->dskaddr) that it needs to be reinserted and placed it back in the active queue.
	 *   In this case, issue another wcs_wtstart to flush the cache. Even if a concurrent writer picks up an
	 *   entry, he should be able to write it out since the journal is already flushed.
	 * The check for whether the cache has been flushed is two-pronged. One via "wcs_active_lvl" and the other
	 *   via the active queue head. Ideally, both are interdependent and checking on "wcs_active_lvl" should be
	 *   enough, but we don't want to take a risk in PRO (in case wcs_active_lvl is incorrect).
	 */
	crq = &csa->acc_meth.bg.cache_state->cacheq_active;
	assert(((0 <= cnl->wcs_active_lvl) && (cnl->wcs_active_lvl || 0 == crq->fl)) || (ENOSPC == wtstart_errno));
	if (cnl->wcs_active_lvl || crq->fl)
	{
		wtstart_errno = wcs_wtstart(gv_cur_region, csd->n_bts);		/* Flush it all */
		WAIT_FOR_CONCURRENT_WRITERS_TO_FINISH(fix_in_wtstart);
		if (cnl->wcs_active_lvl || crq->fl)		/* give allowance in PRO */
		{
			if (ENOSPC == wtstart_errno)
			{	/* wait for csd->wait_disk_space seconds, and give up if still not successful */
				to_wait = csd->wait_disk_space;
				to_msg = (to_wait / 8) ? (to_wait / 8) : 1; /* send message 8 times */
				while ((0 < to_wait) && (ENOSPC == wtstart_errno))
				{
					if ((to_wait == csd->wait_disk_space)
						|| (0 == to_wait % to_msg))
					{
						send_msg(VARLSTCNT(7) ERR_WAITDSKSPACE, 4,
							process_id, to_wait, DB_LEN_STR(gv_cur_region), wtstart_errno);
						gtm_putmsg(VARLSTCNT(7) ERR_WAITDSKSPACE, 4,
							process_id, to_wait, DB_LEN_STR(gv_cur_region), wtstart_errno);
					}
					hiber_start(1000);
					to_wait--;
					wtstart_errno = wcs_wtstart(gv_cur_region, csd->n_bts);
					if (0 == crq->fl)
						break;
				}
				if ((to_wait <= 0) && (cnl->wcs_active_lvl || crq->fl))
				{	/* not enough space became available after the wait */
					send_msg(VARLSTCNT(5) ERR_OUTOFSPACE, 3, DB_LEN_STR(gv_cur_region), process_id);
					rts_error(VARLSTCNT(5) ERR_OUTOFSPACE, 3, DB_LEN_STR(gv_cur_region), process_id);
				}
			} else
			{
				SET_TRACEABLE_VAR(csd->wc_blocked, TRUE);
				BG_TRACE_PRO_ANY(csa, wcb_wcs_flu1);
				send_msg(VARLSTCNT(8) ERR_WCBLOCKED, 6, LEN_AND_LIT("wcb_wcs_flu1"),
                        		process_id, csa->ti->curr_tn, DB_LEN_STR(gv_cur_region));
				assert(!jnl_enabled || jb->fsync_dskaddr == jb->freeaddr);
				wcs_recover(gv_cur_region);
				if (jnl_enabled && (jb->fsync_dskaddr != jb->freeaddr))
				{	/* an INCTN record should have been written above */
					assert(jb->fsync_dskaddr <= jb->dskaddr);
					assert((jb->dskaddr - jb->fsync_dskaddr) >= INCTN_RECLEN);
						/* above assert has a >= instead of == due to possible ALIGN record in between */
					jnl_flush(gv_cur_region);
					assert(jb->freeaddr == jb->dskaddr);
					jnl_fsync(gv_cur_region, jb->dskaddr);
					assert(jb->fsync_dskaddr == jb->dskaddr);
				}
				wcs_wtstart(gv_cur_region, csd->n_bts);		/* Flush it all */
				WAIT_FOR_CONCURRENT_WRITERS_TO_FINISH(fix_in_wtstart);
				if (cnl->wcs_active_lvl || crq->fl)
				{
					cnl->wcsflu_pid = 0;
					if (!was_crit)
						rel_crit(gv_cur_region);
					GTMASSERT;
				}
			}
		}
	}
	if (flush_hdr)
	{
		assert(memcmp(csd->label, GDS_LABEL, GDS_LABEL_SZ - 1) == 0);
		fileheader_sync(gv_cur_region);
	}
	if (jnl_enabled && write_epoch && jb->before_images)
	{	/* If need to write an epoch,
		 *	(1) get hold of the jnl io_in_prog lock.
		 *	(2) set need_db_fsync to TRUE in the journal buffer.
		 *	(3) release the jnl io_in_prog lock.
		 *	(4) write an epoch record in the journal buffer.
		 * The next call to jnl_qio_start() will do the fsync() of the db before doing any jnl qio.
		 * The basic requirement is that we shouldn't write the epoch out until we have synced the database.
		 */
		assert(jb->fsync_dskaddr == jb->freeaddr);
		/* If jb->need_db_fsync is TRUE at this point of time, it means we already have a db_fsync waiting
		 * to happen. This means the epoch due to the earlier need_db_fsync hasn't yet been written out to
		 * the journal file. But that means we haven't yet flushed the journal buffer which leads to a
		 * contradiction. (since we have called jnl_flush earlier in this routine and also assert to the
		 * effect jb->fsync_dskaddr == jb->freeaddr a few lines above).
		 */
		assert(!jb->need_db_fsync);
		for (lcnt = 1; FALSE == (GET_SWAPLOCK(&jb->io_in_prog_latch)); lcnt++)
		{
			if (MAXJNLQIOLOCKWAIT == lcnt)	/* tried too long */
			{
				assert(FALSE);
				cnl->wcsflu_pid = 0;
				if (!was_crit)
					rel_crit(gv_cur_region);
				GTMASSERT;
			}
			wcs_sleep(SLEEP_JNLQIOLOCKWAIT);	/* since it is a short lock, sleep the minimum */
			performCASLatchCheck(&jb->io_in_prog_latch, lcnt);
		}
		jb->need_db_fsync = TRUE;	/* for comments on need_db_fsync, see jnl_output_sp.c */
		RELEASE_SWAPLOCK(&jb->io_in_prog_latch);
		assert(!(JNL_FILE_SWITCHED(jpc)));
		if (!jgbl.dont_reset_gbl_jrec_time && (csa->ti->curr_tn == csa->ti->early_tn))
			JNL_SHORT_TIME(jgbl.gbl_jrec_time);	/* needed for jnl_put_jrt_pini() and jnl_write_epoch_rec() */
		assert(jgbl.gbl_jrec_time);
		if (0 == csa->jnl->pini_addr)
			jnl_put_jrt_pini(csa);
		jnl_write_epoch_rec(csa);
	}
	cnl->wcsflu_pid = 0;
	if (!was_crit)
		rel_crit(gv_cur_region);
	/* sync the epoch record in the journal if needed. */
	if (jnl_enabled && jb->before_images && write_epoch && sync_epoch && csa->ti->curr_tn == csa->ti->early_tn)
	{	/* Note that if we are in the midst of committing and came here through a bizarre
		 * stack trace (like wcs_get_space etc.) we want to defer syncing to when we go out of crit.
		 * Note that we are guaranteed to come back to wcs_wtstart since we are currently in commit-phase
		 * and will dirty atleast one block as part of the commit for a wtstart timer to be triggered.
		 */
		jnl_wait(gv_cur_region);
	}
	if (need_db_fsync && JNL_ALLOWED(csd))
		DB_FSYNC(gv_cur_region, udi, csa, db_fsync_in_prog);
	return ret;
}
