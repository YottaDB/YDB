/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>	/* for VSIG_ATOMIC_T type */
#include "gtm_time.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdsblk.h"
#include "gt_timer.h"
#include "jnl.h"
#include "interlock.h"
#include "error.h"
#include "gdsbgtr.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "aswp.h"
#include "eintr_wrappers.h"
#include "util.h"
#include "send_msg.h"
#include "change_reg.h"
#include "compswap.h"
#ifdef UNIX
#include "mutex.h"
#endif
#include "gds_rundown.h"
#include "gvusr.h"
#include "do_semop.h"
#include "mmseg.h"
#include "ipcrmid.h"
#include "gtmmsg.h"
#include "wcs_recover.h"
#include "tp_change_reg.h"
#include "wcs_flu.h"
#include "add_inter.h"
#include "io.h"
#include "gtmsecshr.h"
#include "ftok_sems.h"
#include "gtmimagename.h"

#define CANCEL_DB_TIMERS(region)						\
{										\
	sgmnt_addrs	*csa;							\
										\
	csa = &FILE_INFO(region)->s_addrs;					\
	if (csa->timer)								\
	{									\
		cancel_timer((TID)region);					\
		if (NULL != csa->nl)						\
			DECR_CNT(&csa->nl->wcs_timers, &csa->nl->wc_var_lock);	\
	}									\
	if (csa->dbsync_timer)							\
	{									\
		cancel_timer((TID)csa);						\
		csa->dbsync_timer = FALSE;					\
	}									\
}

GBLREF	VSIG_ATOMIC_T		forced_exit;
GBLREF	boolean_t		mupip_jnl_recover;
GBLREF	boolean_t		created_core, need_core, dont_want_core, is_src_server, is_rcvr_server, is_updproc;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	uint4			process_id;
GBLREF	ipcs_mesg		db_ipcs;
GBLREF	enum gtmImageTypes	image_type;
GBLREF	jnl_process_vector	*prc_vec;
GBLREF	jnl_process_vector	*originator_prc_vec;

static boolean_t		grabbed_access_sem;

void gds_rundown(void)
{

	bool			is_mm, we_are_last_user, we_are_last_writer;
	boolean_t		ipc_deleted, remove_shm;
	char			*time_ptr;
	gd_region		*reg;
	int			save_errno, status;
	int4			semval, ftok_semval, sopcnt, ftok_sopcnt;
	short			crash_count;
	sm_long_t		munmap_len;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	struct shmid_ds		shm_buf;
	struct sembuf		sop[2], ftok_sop[2];
	time_t  		now;
	uint4           	jnl_status;
	unix_db_info		*udi;

	error_def(ERR_CRITSEMFAIL);
	error_def(ERR_DBFILERR);
	error_def(ERR_DBRNDWNWRN);
	error_def(ERR_DBCCERR);
	error_def(ERR_ERRCALL);
	error_def(ERR_RECOVIPCDEL);
	error_def(ERR_RNDWNSEMFAIL);
	error_def(ERR_TEXT);
	error_def(ERR_WCBLOCKED);
	error_def(ERR_GTMASSERT);

	forced_exit = FALSE;		/* Okay, we're dying already -- let rel_crit live in peace now.
					 * If coming through a DAL, not necessarily dying. what to do then? -- nars -- 8/15/2001
					 */
	grabbed_access_sem = FALSE;
	jnl_status = 0;
	reg = gv_cur_region;			/* Local copy */

	/*
	 * early out for cluster regions
	 * to avoid tripping the assert below.
	 * Note:
	 *	This early out is consistent with VMS.  It has been
	 *	noted that all of the gtcm assignments
	 *      to gv_cur_region should use the TP_CHANGE_REG
	 *	macro.  This would also avoid the assert problem
	 *	and should be done eventually.
	 */
	if (dba_cm == reg->dyn.addr->acc_meth)
		return;

	udi = FILE_INFO(reg);
	csa = &udi->s_addrs;
	csd = csa->hdr;
	assert(csa == cs_addrs && csd == cs_data);
	if ((reg->open) && (dba_usr == csd->acc_meth))
	{
		change_reg();
		gvusr_rundown();
		return;
	}
	ESTABLISH(gds_rundown_ch);
	if (!reg->open)				/* Not open, no point to rundown */
	{
		if (reg->opening)		/* Died partway open, kill rest of way */
		{
			rel_crit(reg);
			mutex_cleanup(reg);
/* revist this to handle MM properly  SMW 98/12/16
                        if (NULL != csa->nl)
                        {
                                status = shmdt((caddr_t)csa->nl);
                                if (-1 == status)
                                        send_msg(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
                                                ERR_TEXT, 2, LEN_AND_LIT("Error during shmdt"), errno);
                        }
*/
			shmdt((caddr_t)csa->nl);
			csa->nl = NULL;
		}
		REVERT;
		return;
	}
	switch(csd->acc_meth)
	{	/* Pass mm and bg through */
	    case dba_bg:
		is_mm = FALSE;
		break;
	    case dba_mm:
		is_mm = TRUE;
		break;
	    case dba_usr:
		assert(FALSE);
	    default:
		REVERT;
		return;
	}
	/* Cancel any pending flush timer for this region by this task */
	CANCEL_DB_TIMERS(reg);
	we_are_last_user = FALSE;
	if (!csa->persistent_freeze)
		region_freeze(reg, FALSE, FALSE);
	if (csa->read_lock)			/* get locks to known state */
	{
		crash_count = csa->critical->crashcnt;
		rel_read_crit(reg, crash_count);
	}
	else
		rel_crit(reg);
	mutex_cleanup(reg);
	/*
	 * We need to guarantee that none else access database file header when semid/shmid fields are reset.
	 * We already have created ftok semaphore in db_init or, mu_rndwn_file and did not remove it.
	 * So just lock it. We do it in blocking mode.
	 */
	if (!ftok_sem_lock(reg, FALSE, FALSE))
		rts_error(VARLSTCNT(4) ERR_DBFILERR, 2, DB_LEN_STR(reg));
	/*
	 * For mupip_jnl_recover we already have database access control semaphore.
	 * We do not release it. We release it from  mur_close_files.
	 */
	if (!mupip_jnl_recover)
	{
		sop[0].sem_num = 0; sop[0].sem_op = 0;	/* Wait for 0 */
		sop[1].sem_num = 0; sop[1].sem_op = 1;	/* Lock */
		sopcnt = 2;
		sop[0].sem_flg = sop[1].sem_flg = SEM_UNDO | IPC_NOWAIT; /* Don't wait the first time thru */
		SEMOP(udi->semid, sop, sopcnt, status);
		if (-1 == status)			/* We couldn't get it in one shot -- see if we already have it */
		{
			save_errno = errno;
			if (semctl(udi->semid, 0, GETPID) == process_id)
			{
				send_msg(VARLSTCNT(5) MAKE_MSG_INFO(ERR_CRITSEMFAIL), 2,
					DB_LEN_STR(reg),
					ERR_RNDWNSEMFAIL);
				REVERT;
				return;			/* Already in rundown for this region */
			}
			if (EAGAIN != save_errno)
				rts_error(VARLSTCNT(9) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg),
					ERR_TEXT, 2, RTS_ERROR_TEXT("gds_rundown first semop/semctl"), save_errno);
			sop[0].sem_flg = sop[1].sem_flg = SEM_UNDO;	/* Try again - blocking this time */
			SEMOP(udi->semid, sop, 2, status);
			if (-1 == status)			/* We couldn't get it at all.. */
				rts_error(VARLSTCNT(5) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg), errno);
		}
	}
	grabbed_access_sem = TRUE;
	/*
	 * We now have the dbinit/rundown lock, so we are alone in this code for this region
	 * and nobody else can attach.
	 * See if we are all alone in accessing this database shared memory.
	 */
	--csa->nl->ref_cnt;
	if (-1 == shmctl(udi->shmid, IPC_STAT, &shm_buf))
	{
		save_errno = errno;
		rts_error(VARLSTCNT(9) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg),
			ERR_TEXT, 2, RTS_ERROR_TEXT("gds_rundown shmctl"), save_errno);
	} else
		we_are_last_user =  (1 == shm_buf.shm_nattch);
	assert(!mupip_jnl_recover || we_are_last_user); /* recover => one user */
	if (-1 == (semval = semctl(udi->semid, 1, GETVAL)))
		rts_error(VARLSTCNT(5) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg), errno);
	we_are_last_writer = (1 == semval && FALSE == reg->read_only);	/* There's one writer left and I am it */
	assert(!mupip_jnl_recover || we_are_last_writer); /* recover => one writer */
	if (-1 == (ftok_semval = semctl(udi->ftok_semid, 1, GETVAL)))
		rts_error(VARLSTCNT(5) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg), errno);
	/* Certain things are only done for writing tasks */
	if (!reg->read_only)
	{
		/* If we had an orphaned block and were interrupted, set wc_blocked so we can invoke wcs_recover */
		if (csa->wbuf_dqd)
		{
			grab_crit(reg);
			SET_TRACEABLE_VAR(csd->wc_blocked, TRUE);
			BG_TRACE_PRO_ANY(csa, wcb_gds_rundown);
                        send_msg(VARLSTCNT(8) ERR_WCBLOCKED, 6, LEN_AND_LIT("wcb_gds_rundown"),
                                process_id, csa->ti->curr_tn, DB_LEN_STR(reg));
			csa->wbuf_dqd = FALSE;
			wcs_recover(reg);
			if (is_mm)
			{
				assert(FALSE);
				csd = csa->hdr;
			}
			BG_TRACE_PRO_ANY(csa, lost_block_recovery);
			rel_crit(reg);
		}
		if (((JNL_ENABLED(csd)) && (GTCM_GNP_SERVER_IMAGE == image_type)) || mupip_jnl_recover)
			originator_prc_vec = prc_vec;
		/* If we are the last writing user, then everything must be flushed */
		if (we_are_last_writer)
		{
			/* Time to flush out all of our buffers */
			if (FALSE == is_mm)
			{
				/* Note WCSFLU_SYNC_EPOCH ensures the epoch is synced to the journal and indirectly
				 * also ensures that the db is fsynced. We don't want to use it in the calls to
				 * wcs_flu() from t_end() and tp_tend() since we can defer it to out-of-crit there.
				 * In this case, since we are running down, we don't have any such option.
				 */
				csa->nl->remove_shm = wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH | WCSFLU_SYNC_EPOCH);
				assert(0 == memcmp(csd->label, GDS_LABEL, GDS_LABEL_SZ - 1));
			} else
			{
				if (csa->total_blks != csa->ti->total_blks)	/* do remap if file had been extended */
				{
					grab_crit(reg);
					wcs_recover(reg);
					csd = csa->hdr;
					rel_crit(reg);
				}
				/* Flush MM mode mapped memory segment */
#if defined(UNTARGETED_MSYNC)
				if (csa->ti->last_mm_sync != csa->ti->curr_tn)
				{
					if (-1 == msync((caddr_t)csa->db_addrs[0],
								(size_t)(csa->db_addrs[1] - csa->db_addrs[0]), MS_SYNC))
					{
						rts_error(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
							  ERR_TEXT, 2, RTS_ERROR_TEXT("Error during file msync at close"), errno);
					}
					csa->ti->last_mm_sync = csa->ti->curr_tn;
				}
#else
				if (0 != csa->acc_meth.mm.mmblk_state->mmblkq_active.fl)
				{
					csa->nl->mm_extender_pid = process_id;	/* to write all at one shot */
					wcs_wtstart(reg, 0);
					if (0 != csa->acc_meth.mm.mmblk_state->mmblkq_active.fl)
					{
						rts_error(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
							  ERR_TEXT, 2, RTS_ERROR_TEXT("Error during write flush at close"), errno);
					}
				}
				csa->nl->mm_extender_pid = 0;
#endif
				csa->nl->remove_shm = TRUE;
			}
			csd->trans_hist.header_open_tn = csd->trans_hist.curr_tn;
		} else if (csa->timer && 0 > csa->nl->wcs_timers)		/* Last timer is running down now, perform flush */
		{
			grab_crit(reg);
			wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH);
			rel_crit(reg);
		}
		/* Do rundown journal processing after buffer flushes since they require jnl to be open */
		if (JNL_ENABLED(csd))
		{	/* the following tp_change_reg() is not needed due to the assert csa == cs_addrs at the beginning
			 * of gds_rundown(), but just to be safe. To be removed by 2002!! --- nars -- 2001/04/25.
			 */
			tp_change_reg();	/* call this because jnl_ensure_open checks cs_addrs rather than gv_cur_region */
			if (csa->jnl->jnl_buff->fsync_in_prog_latch.latch_pid == process_id)
                        {
                                assert(FALSE);
                                compswap(&csa->jnl->jnl_buff->fsync_in_prog_latch, process_id, LOCK_AVAILABLE);
                        }
                        if (csa->jnl->jnl_buff->io_in_prog_latch.latch_pid == process_id)
                        {
                                assert(FALSE);
                                compswap(&csa->jnl->jnl_buff->io_in_prog_latch, process_id, LOCK_AVAILABLE);
                        }
			if ((0 != csd->jnl_file.u.inode) && (NOJNL != csa->jnl->channel || we_are_last_writer))
			/* If a file has actually been opened .. */
			{
				grab_crit(reg);
				jnl_status = jnl_ensure_open();
				if (0 == jnl_status)
				{
					/* If we_are_last_writer, we would have already done a wcs_flu() which would
					 * have written an epoch record and we are guaranteed no further updates
					 * since we are the last writer. So, just close the journal.
					 * Although we assert pini_addr should be non-zero for last_writer, we
					 * play it safe in PRO and write a PINI record if not written already.
					 */
					assert(!csa->jnl->jnl_buff->before_images || is_mm
							|| !we_are_last_writer || 0 != csa->jnl->pini_addr);
					if (we_are_last_writer && 0 == csa->jnl->pini_addr)
						jnl_put_jrt_pini(csa);
					if (0 != csa->jnl->pini_addr)
						jnl_put_jrt_pfin(csa);
					jnl_file_close(reg, we_are_last_writer, FALSE);
				} else
					send_msg(VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(csd), DB_LEN_STR(reg));
				rel_crit(reg);
			}
		}
		if (we_are_last_writer)			/* Flush the fileheader last and harden the file to disk */
		{
			grab_crit(reg);			/* To satisfy crit requirement in fileheader_sync() */
			memset(csd->machine_name, 0, MAX_MCNAMELEN); /* clear the machine_name field */
			if (!mupip_jnl_recover && we_are_last_user)
			{
				/* mupip_jnl_recover will do this after mur_close_file */
				csd->semid = INVALID_SEMID;
				csd->shmid = INVALID_SHMID;
				csd->sem_ctime.ctime = 0;
				csd->shm_ctime.ctime = 0;
			}
			fileheader_sync(reg);
			rel_crit(reg);
			if (FALSE == is_mm)
			{
				if (-1 == fsync(udi->fd))		/* Sync it all */
				{
					rts_error(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
						  ERR_TEXT, 2, RTS_ERROR_TEXT("Error during file sync at close"), errno);
				}
			} else
			{	/* Now do final MM file sync before exit */
#ifndef TARGETED_MSYNC
				if (-1 == fsync(udi->fd))		/* Sync it all */
				{
					rts_error(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
						  ERR_TEXT, 2, RTS_ERROR_TEXT("Error during file sync at close"), errno);
				}
#else
				if (-1 == msync((caddr_t)csa->db_addrs[0], (size_t)(csa->db_addrs[1] - csa->db_addrs[0]), MS_SYNC))
				{
					rts_error(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
						  ERR_TEXT, 2, RTS_ERROR_TEXT("Error during file msync at close"), errno);
				}
#endif
			}
                }
	} /* end if read-write */
	if (reg->read_only && we_are_last_user && !mupip_jnl_recover)
	{
		/* mupip_jnl_recover will do this after mur_close_file */
		db_ipcs.semid = INVALID_SEMID;
		db_ipcs.shmid = INVALID_SHMID;
		db_ipcs.sem_ctime = 0;
		db_ipcs.shm_ctime = 0;
		db_ipcs.fn_len = reg->dyn.addr->fname_len;
		memcpy(db_ipcs.fn, reg->dyn.addr->fname, reg->dyn.addr->fname_len);
		db_ipcs.fn[reg->dyn.addr->fname_len] = 0;
 		/*
		 * request gtmsecshr to flush. read_only cannot flush itself.
		 */
		if (0 != send_mesg2gtmsecshr(FLUSH_DB_IPCS_INFO, 0, (char *)NULL, 0))
			rts_error(VARLSTCNT(8) ERR_DBFILERR, 2, DB_LEN_STR(reg),
				  ERR_TEXT, 2, RTS_ERROR_TEXT("gtmsecshr failed to update database file header"));
	}
	/* Done with file now, close it */
	if (-1 == close(udi->fd))
	{
		rts_error(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
			  ERR_TEXT, 2, LEN_AND_LIT("Error during file close"), errno);
	}
	/* Unmap storage if mm mode but only the part that is not the fileheader (so shows up in dumps) */
	if (is_mm)
	{
		munmap_len = (sm_long_t)(csa->db_addrs[1] - csa->db_addrs[0]) - ROUND_UP(sizeof(sgmnt_data), MSYNC_ADDR_INCS);
		if (munmap_len > 0)
		{
			munmap((caddr_t)(csa->db_addrs[0] + ROUND_UP(sizeof(sgmnt_data), MSYNC_ADDR_INCS)), (size_t)(munmap_len));
#ifdef DEBUG_DB64
			rel_mmseg((caddr_t)csa->db_addrs[0]);
#endif
		}
	}
	/* Detach our shared memory while still under lock so reference counts will be
	 * correct for the next process to run down this region.
	 * In the process also get the remove_shm status from node_local before detaching.
	 */
	remove_shm = csa->nl->remove_shm;
	status = shmdt((caddr_t)csa->nl);
	csa->nl = NULL; /* dereferencing nl after detach is not right, so we set it to NULL so that we can test before dereference*/
	if (-1 == status)
		send_msg(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg), ERR_TEXT, 2, LEN_AND_LIT("Error during shmdt"), errno);
	reg->open = FALSE;
	if (csa->wbuf_dqd)			/* If file is still not in good shape, die here and now */
		GTMASSERT;			/* .. before we get rid of our storage */
	assert (!is_rcvr_server);
	ipc_deleted = FALSE;
	/* If we are the very last user, remove shared storage id and the semaphores */
	if (we_are_last_user)
	{
		/* remove shared storage, only if last writer to rundown did a successful wcs_flu() */
		if (remove_shm)
		{
			ipc_deleted = TRUE;
			if (0 != shm_rmid(udi->shmid))
				rts_error(VARLSTCNT(8) ERR_DBFILERR, 2, DB_LEN_STR(reg),
					ERR_TEXT, 2, RTS_ERROR_TEXT("Unable to remove shared memory"));
		} else if (is_src_server || is_updproc)
		{
			gtm_putmsg(VARLSTCNT(6) ERR_DBRNDWNWRN, 4, DB_LEN_STR(reg), process_id, process_id);
			send_msg(VARLSTCNT(6) ERR_DBRNDWNWRN, 4, DB_LEN_STR(reg), process_id, process_id);
		} else
			send_msg(VARLSTCNT(6) ERR_DBRNDWNWRN, 4, DB_LEN_STR(reg), process_id, process_id);
		/*
		 * Don't release semaphore in case of mupip recover/rollback; since it has standalone access.
		 * It will release the semaphore in mur_close_files.
		 */
		if (!mupip_jnl_recover)
		{
			if (0 != sem_rmid(udi->semid))
				rts_error(VARLSTCNT(8) ERR_DBFILERR, 2, DB_LEN_STR(reg),
					ERR_TEXT, 2, RTS_ERROR_TEXT("Unable to remove semaphore"));
			grabbed_access_sem = FALSE;
		}
	} else
	{
		assert(!mupip_jnl_recover);
		/* If we were writing, get rid of our writer access count semaphore */
		if (!reg->read_only)
			if (0 != (save_errno = do_semop(udi->semid, 1, -1, SEM_UNDO)))
				rts_error(VARLSTCNT(9) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg),
					ERR_TEXT, 2, RTS_ERROR_TEXT("gds_rundown write semaphore release"), save_errno);
		/* Now remove the rundown lock */
		if (0 != (save_errno = do_semop(udi->semid, 0, -1, SEM_UNDO)))
			rts_error(VARLSTCNT(9) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg),
				ERR_TEXT, 2, RTS_ERROR_TEXT("gds_rundown rundown semaphore release"), save_errno);
		grabbed_access_sem = FALSE;
	}
	if (!ftok_sem_release(reg, !mupip_jnl_recover, FALSE))
			rts_error(VARLSTCNT(4) ERR_DBFILERR, 2, DB_LEN_STR(reg));
	GET_CUR_TIME;
	if (is_src_server)
		util_out_print("!AD : Source server !AZ IPC resources for region !AD", TRUE,
				CTIME_BEFORE_NL, time_ptr, ipc_deleted ? "deleted" : "did not delete", REG_LEN_STR(reg));
	if (is_updproc)
		util_out_print("!AD : Update process !AZ IPC resources for region !AD", TRUE,
				CTIME_BEFORE_NL, time_ptr, ipc_deleted ? "deleted" : "did not delete", REG_LEN_STR(reg));
	if (mupip_jnl_recover)
	{
                send_msg(VARLSTCNT(7) ERR_RECOVIPCDEL, 5, CTIME_BEFORE_NL, time_ptr,
				ipc_deleted ? "deleted" : "did not delete", DB_LEN_STR(reg));
		if(!ipc_deleted)
			util_out_print("!AD : Recover process did not delete IPC resources for database !AD", TRUE,
					CTIME_BEFORE_NL, time_ptr, DB_LEN_STR(reg));
	}
	REVERT;
}

CONDITION_HANDLER(gds_rundown_ch)
{
	pid_t		sem_pid;
	int		semop_res;
	unix_db_info	*udi;
	sgmnt_addrs	*csa;
	void		rel_crit();

	error_def(ERR_ASSERT);
	error_def(ERR_CRITSEMFAIL);
	error_def(ERR_DBRNDWN);
	error_def(ERR_GTMASSERT);
	error_def(ERR_OUTOFSPACE);
	error_def(ERR_STACKOFLOW);
	error_def(ERR_GTMCHECK);
	error_def(ERR_TEXT);

	START_CH;
	/* To get as virgin a state as possible in the core, take the core now if we
	   would be doing so anyway. This will set created_core so it doesn't happen again.
	*/
	if (DUMPABLE && !SUPPRESS_DUMP)
	{
		need_core = TRUE;
		gtm_fork_n_core();
	}
	udi = FILE_INFO(gv_cur_region);
	csa = &udi->s_addrs;
	/* We got here on an error and are going to close the region. Cancel any pending flush timer for this region by this task*/
	CANCEL_DB_TIMERS(gv_cur_region);
	/* release the access control semaphore, if you hold it */
	if (grabbed_access_sem)
	{
		if (csa->now_crit)		/* Might hold crit if wcs_flu or other failure */
		{
			if (NULL != csa->nl)
				rel_crit(gv_cur_region); /* also sets csa->now_crit to FALSE */
			else
				csa->now_crit = FALSE;
		}
		sem_pid = semctl(udi->semid, 0, GETPID);
		assert(sem_pid == process_id);
		if (0 != (semop_res = do_semop(udi->semid, 0, -1, SEM_UNDO | IPC_NOWAIT)))
			gtm_putmsg(VARLSTCNT(9) ERR_CRITSEMFAIL, 2, DB_LEN_STR(gv_cur_region),
					  ERR_TEXT, 2, RTS_ERROR_TEXT("Error releasing access semaphore"), semop_res);

	}
	if (udi->grabbed_ftok_sem)
		ftok_sem_release(gv_cur_region, !mupip_jnl_recover, TRUE);
	gv_cur_region->open = FALSE;
	PRN_ERROR;
	gtm_putmsg(VARLSTCNT(4) ERR_DBRNDWN, 2, REG_LEN_STR(gv_cur_region));
	UNWIND(NULL, NULL);
}
