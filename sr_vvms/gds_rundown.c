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

#include <descrip.h>
#include <fab.h>
#include <lckdef.h>
#include <iodef.h>
#include <psldef.h>
#include <secdef.h>
#include <ssdef.h>
#include <efndef.h>

#include "gtm_time.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "ast.h"
#include "ccp.h"
#include "ccpact.h"
#include "cdb_sc.h"
#include "efn.h"
#include "jnl.h"
#include "timers.h"
#include "error.h"
#include "iosp.h"
#include "locks.h"
#include "util.h"
#include "send_msg.h"
#include "change_reg.h"
#include "is_proc_alive.h"
#include "del_sec.h"
#include "mem_list.h"
#include "gvusr.h"
#include "gtmmsg.h"
#include "wcs_recover.h"
#include "wcs_flu.h"
#include "gtmimagename.h"
#include "iosb_disk.h"
#include "wbox_test_init.h"
#include "shmpool.h"	/* needed for shmpool structures */

GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	int4			exi_condition;
GBLREF	uint4			image_count, process_id;
GBLREF	short			crash_count, astq_dyn_avail;
GBLREF	boolean_t       	is_src_server;
GBLREF	boolean_t		is_rcvr_server;
GBLREF	boolean_t		is_updproc;
GBLREF  boolean_t		mupip_jnl_recover;
GBLREF	jnl_process_vector	*originator_prc_vec;
GBLREF	jnl_process_vector	*prc_vec;
GBLREF 	jnl_gbls_t		jgbl;
GBLREF	boolean_t		mu_rndwn_process;
GBLREF	boolean_t		dse_running;
#ifdef DEBUG
GBLREF	boolean_t		in_mu_rndwn_file;
#endif

LITREF	char			gtm_release_name[];
LITREF	int4			gtm_release_name_len;

static	const	unsigned short	zero_fid[3];

error_def(ERR_ASSERT);
error_def(ERR_CRITRESET);
error_def(ERR_DBRNDWN);
error_def(ERR_DBRNDWNWRN);
error_def(ERR_GTMASSERT);
error_def(ERR_GTMASSERT2);
error_def(ERR_GTMCHECK);
error_def(ERR_IPCNOTDEL);
error_def(ERR_JNLFLUSH);
error_def(ERR_OUTOFSPACE);
error_def(ERR_STACKOFLOW);
error_def(ERR_TEXT);
error_def(ERR_VMSMEMORY);

CONDITION_HANDLER(gds_rundown_ch)
{
	vms_gds_info		*gds_info;
	jnl_private_control	*jpc = NULL;
	uint4			outaddrs[2], retadr[2], status;
	boolean_t		nonclst_bg;

        START_CH;

	if (PRO_ONLY(mu_rndwn_process &&) DUMPABLE && !SUPPRESS_DUMP)
		TERMINATE;
	/* deq all the locks, so it wont affect other processes */
	gds_info = gv_cur_region->dyn.addr->file_cntl->file_info;
	cs_addrs = &gds_info->s_addrs;
	DEBUG_ONLY(in_mu_rndwn_file = FALSE);
	TREF(donot_write_inctn_in_wcs_recover) = FALSE; /* might have been set to TRUE in mu_rndwn_file(). */
	if (cs_addrs)
	{
		if (cs_addrs->now_crit)
			rel_crit(gv_cur_region);
		jpc = cs_addrs->jnl;
	}
	if (jpc && jpc->jnllsb && 0 != jpc->jnllsb->lockid)
	{
		status = gtm_deq(jpc->jnllsb->lockid, NULL, PSL$C_USER, 0);
		assert(SS$_NORMAL == status);
		jpc->jnllsb->lockid = 0;
	}
	if (0 != gds_info->cx_cntl_lsb.lockid)
	{
		status = gtm_deq(gds_info->cx_cntl_lsb.lockid, NULL, PSL$C_USER, 0);
		assert(SS$_NORMAL == status);
		gds_info->cx_cntl_lsb.lockid = 0;
	}
	if (0 != gds_info->file_cntl_lsb.lockid)
	{
		status = gtm_deq(gds_info->file_cntl_lsb.lockid, NULL, PSL$C_USER, 0);
		assert(SS$_NORMAL == status);
		gds_info->file_cntl_lsb.lockid = 0;
	}

	/* -------------------------- take care of address space ------------------------- */
	if (cs_addrs)
	{
		if (cs_addrs->hdr)
		{
			nonclst_bg = (dba_bg == cs_addrs->hdr->acc_meth);
			if (FALSE == nonclst_bg)
			{
				cs_addrs->lock_addrs[1] = (sm_uc_ptr_t)(cs_addrs->nl) + ROUND_UP(LOCK_SPACE_SIZE(cs_addrs->hdr)
								+ NODE_LOCAL_SPACE(cs_addrs->hdr) + JNL_SHARE_SIZE(cs_addrs->hdr)
								+ SHMPOOL_BUFFER_SIZE + 1, OS_PAGE_SIZE) + 1;
				cs_addrs->lock_addrs[0] = (sm_uc_ptr_t)(cs_addrs->nl);
				if (FALSE == is_va_free(cs_addrs->lock_addrs[0]))
					gtm_deltva(cs_addrs->lock_addrs, retadr, PSL$C_USER);
			}
		}
		outaddrs[0] = cs_addrs->db_addrs[0] - OS_PAGE_SIZE;	/* header no access page */
		outaddrs[1] = cs_addrs->db_addrs[1] + OS_PAGE_SIZE;	/* trailer no access page */
		if (FALSE == is_va_free(outaddrs[0]))
			gtm_deltva(outaddrs, retadr, PSL$C_USER);
	}

	sys$dassgn(gds_info->fab->fab$l_stv);
	gv_cur_region->open = FALSE;
	if (cs_addrs)
	{
		cs_addrs->hdr = NULL;
		cs_addrs->nl = NULL;
		REMOVE_CSA_FROM_CSADDRSLIST(cs_addrs);	/* remove "cs_addrs" from list of open regions (cs_addrs_list) */
		cs_addrs->db_addrs[0] = NULL;
		cs_addrs->lock_addrs[0] = NULL;
	}
        PRN_ERROR;
	gtm_putmsg(VARLSTCNT(4) ERR_DBRNDWN, 2, REG_LEN_STR(gv_cur_region));
        UNWIND(NULL, NULL);
}

void	gds_rundown(void)
{
	bool			nonclst_bg, clustered, read_write;
	char			name_buff[GLO_NAME_MAXLEN];
	io_status_block_disk	iosb;
	uint4			jnl_status, last_one_status, outaddrs[2], retadr[2], status, we_are_last_writer;
	vms_gds_info		*gds_info;
	boolean_t		ipc_deleted, remove_shm, cancelled_timer, cancelled_dbsync_timer, vermismatch;
	now_t			now; 						/* for GET_CUR_TIME macro*/
	char			*time_ptr, time_str[CTIME_BEFORE_NL + 2]; 	/* for GET_CUR_TIME macro*/
	jnl_private_control	*jpc;
	jnl_buffer_ptr_t	jbp;
	$DESCRIPTOR(desc, name_buff);

	jnl_status = 0;
	if (!gv_cur_region->open)
		return;
	if (dba_usr == gv_cur_region->dyn.addr->acc_meth)
	{
		change_reg();
		gvusr_rundown();
		return;
	}
	if ((dba_bg != gv_cur_region->dyn.addr->acc_meth) && (dba_mm != gv_cur_region->dyn.addr->acc_meth))
		return;
	ESTABLISH(gds_rundown_ch);
	gds_info = gv_cur_region->dyn.addr->file_cntl->file_info;
	cs_addrs = &gds_info->s_addrs;
	assert(cs_data == cs_addrs->hdr);
	clustered = cs_addrs->hdr->clustered;
	crash_count = cs_addrs->critical->crashcnt;
	last_one_status = we_are_last_writer = FALSE;
	nonclst_bg = (dba_bg == cs_addrs->hdr->acc_meth);
	read_write = (FALSE == gv_cur_region->read_only);
	if (!cs_addrs->persistent_freeze)
		region_freeze(gv_cur_region, FALSE, FALSE, FALSE);
	if (ERR_CRITRESET != exi_condition)
		rel_crit(gv_cur_region);
	sys$cantim(gv_cur_region, PSL$C_USER);	/* cancel all database write timers and jnl_mm_timer()s for this region */
	cancelled_timer = FALSE;
	if (cs_addrs->timer)
	{
		adawi(-1, &cs_addrs->nl->wcs_timers);
		cs_addrs->timer = FALSE;
		cancelled_timer = TRUE;
		++astq_dyn_avail;
	}
	/* The order of resetting cs_addrs->dbsync_timer and the sys$cantim(cs_addrs) following it is important.
	 * This is because once dbsync_timer is set to FALSE, both wcs_clean_dbsync_timer_ast and wcs_clean_dbsync_ast
	 * 	know not to proceed with the dbsync any further.
	 * If we cancelled the timer first, then it is possible that before resetting dbsync_timer to FALSE, we
	 *	got interrupted by wcs_clean_dbsync_timer_ast which might then set off a timer to drive
	 *	wcs_clean_dbsync_ast only to have dbsync_timer reset to FALSE by the next statement in gds_rundown()
	 *	effectively ending up in a situation where we will have an orphaned wcs_clean_dbsync_ast timer pop
	 *	much later after the region has been rundown which we don't want.
	 */
	cancelled_dbsync_timer = FALSE;
	if (cs_addrs->dbsync_timer)
	{
		cs_addrs->dbsync_timer = FALSE;
		cancelled_dbsync_timer = TRUE;
		++astq_dyn_avail;
	}
	sys$cantim(cs_addrs, PSL$C_USER);	/* cancel all epoch-timers for this region */
	jpc = cs_addrs->jnl;
	if (JNL_ENABLED(cs_addrs->hdr) && IS_GTCM_GNP_SERVER_IMAGE);
		originator_prc_vec = NULL;
	if (memcmp(cs_addrs->nl->now_running, gtm_release_name, gtm_release_name_len + 1))
	{	/* VERMISMATCH condition. Possible only if DSE */
		assert(dse_running);
		vermismatch = TRUE;
	} else
		vermismatch = FALSE;
	if (TRUE == clustered)
	{
		nonclst_bg = FALSE;
		if (JNL_ENABLED(cs_addrs->hdr) && (NULL != jpc) && (NOJNL != jpc->channel)
			&& (0 != memcmp(cs_addrs->nl->jnl_file.jnl_file_id.fid, zero_fid, SIZEOF(zero_fid))))
		{
			if (0 != jpc->pini_addr)
			{
				grab_crit(gv_cur_region);
				if (JNL_ENABLED(cs_addrs->hdr))
				{
					jnl_status = jnl_ensure_open();
					if (jnl_status == 0)
					{
						jnl_put_jrt_pfin(cs_addrs);
						if (SS_NORMAL != (jnl_status = jnl_flush(gv_cur_region)))
						{
							send_msg(VARLSTCNT(9) ERR_JNLFLUSH, 2, JNL_LEN_STR(cs_data),
								ERR_TEXT, 2,
									RTS_ERROR_TEXT("Error with journal flush in gds_rundown1"),
								jnl_status);
							assert(NOJNL == jpc->channel);/* jnl file lost has been triggered */
							/* In this routine, all code that follows from here on does not
							 * assume anything about the journaling characteristics of this
							 * database so it is safe to continue execution even though
							 * journaling got closed in the middle.
							 */
						}
					}
				}
				rel_crit(gv_cur_region);
			}
			if (!is_src_server)
			{
				assert(0 != jpc->jnllsb->lockid);
				status = gtm_deq(jpc->jnllsb->lockid, NULL, PSL$C_USER, 0);
				assert(SS$_NORMAL == status);
				jpc->jnllsb->lockid = 0;
			}
			sys$dassgn(jpc->channel);
		}
	} else
	{	/* ----------------------------- acquire startup/shutdown lock ---------------------------------- */
		/* Acquire PW mode to ensure that this is the only process in this section of the rundown code.
		 * Then attempt to get EX mode;  if granted, then this process is the only process accessing the
		 * database, and cache rundown and header flushing should occur.  Use the NOQUEUE flag so that
		 * we don't have to wait for the lock.
		 */
		status = gtm_enqw(EFN$C_ENF, LCK$K_PWMODE, &gds_info->file_cntl_lsb, LCK$M_CONVERT | LCK$M_NODLCKBLK,
			NULL, 0, NULL, 0, NULL, PSL$C_USER, 0);
		if (SS$_NORMAL == status)
			status = gds_info->file_cntl_lsb.cond;
		if (SS$_NORMAL == status)
		{
			status = gtm_enqw(EFN$C_ENF, LCK$K_EXMODE, &gds_info->file_cntl_lsb,
						LCK$M_CONVERT | LCK$M_NOQUEUE | LCK$M_NODLCKWT,
						NULL, 0, NULL, 0, NULL, PSL$C_USER, 0);
			if ((SS$_NORMAL == status) && !vermismatch)
				last_one_status = status = gds_info->file_cntl_lsb.cond;
		}
		if (read_write)
		{
			assert(cs_addrs->nl->ref_cnt > 0);
			assert(cs_addrs->ref_cnt);
			/* The primary purpose of the shared ref_cnt flag is to indicate whether we are the last writer.
			 * The private ref_cnt and shared ref_cnt should always be updated in the following order.
			 * While incrementing, shared ref_cnt should be updated first
			 * While decrementing, shared ref_cnt should be updated last
			 * This way, it is guaranteed that whenever the private ref_cnt is TRUE, this process
			 * 	definitely has incremented the shared ref_cnt.
			 * The only issue with this is that it is possible that the shared ref_cnt has been incremented
			 * 	although the private ref_cnt has not been (due to STOP/ID or kill -9) or when the private
			 * 	ref_cnt has been decremented, while the shared ref_cnt has not been.
			 * In this case, the actual last writer might conclude he is not actually the last writer and
			 * 	hence might not do the necessary cleanup (like writing an EOF record in the journal file etc.)
			 * 	although a later MUPIP RUNDOWN will take care of doing the necessary stuff.
			 * If the ordering of updates of the shared and private ref_cnt were reversed, we have the issue of
			 * 	some process incorrectly concluding itself as the last writer and doing cleanup when it
			 * 	should not be doing so. That is dangerous and can cause damage to the database/journal.
			 */
			if (cs_addrs->ref_cnt)
			{
				cs_addrs->ref_cnt--;
				assert(!cs_addrs->ref_cnt);
				adawi(-1, &cs_addrs->nl->ref_cnt);
			}
		}
		/* ------------------------------ flush if applicable ------------------------------------------
		 * If cs_addrs->nl->donotflush_dbjnl is set, it means mupip recover/rollback was interrupted and therefore we
		 * 	should not flush shared memory contents to disk as they might be in an inconsistent state.
		 * In this case, we will go ahead and remove shared memory (without flushing the contents) in this routine.
		 * A reissue of the recover/rollback command will restore the database to a consistent state.
		 */
		if (read_write && !cs_addrs->nl->donotflush_dbjnl && !vermismatch)
		{
			if (cs_addrs->nl->ref_cnt < 1)
			{	/* since csa->nl->ref_cnt is not an accurate indicator of whether we are the last writer or not,
				 * we need to be careful before modifying any shared memory fields below, hence the grab_crit */
				grab_crit(gv_cur_region);
				cs_addrs->nl->ref_cnt = 0;	/* just in case for pro */
				we_are_last_writer = TRUE;
				/* ========================= laster writer in this region ================= */
				if (nonclst_bg)
				{	/* Note WCSFLU_SYNC_EPOCH ensures the epoch is synced to the journal and indirectly
					 * also ensures that the db is fsynced. We don't want to use it in the calls to
					 * wcs_flu() from t_end() and tp_tend() since we can defer it to out-of-crit there.
					 * In this case, since we are running down, we don't have any such option.
					 */
					cs_addrs->nl->remove_shm =
							wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH | WCSFLU_SYNC_EPOCH);
				}
				assert(0 == memcmp(cs_addrs->hdr->label, GDS_LABEL, GDS_LABEL_SZ - 1));
				cs_addrs->hdr->owner_node = 0;
				memset(cs_addrs->hdr->now_running, 0, SIZEOF(cs_addrs->hdr->now_running));
				if (nonclst_bg)
				{
					sys$qiow(EFN$C_ENF, gds_info->fab->fab$l_stv, IO$_WRITEVBLK, &iosb, NULL, 0,
							cs_addrs->hdr, SIZEOF(sgmnt_data), 1, 0, 0, 0);
				} else
				{
					if (SS$_NORMAL == sys$updsec(cs_addrs->db_addrs, NULL, PSL$C_USER, 0,
							efn_immed_wait, &iosb, NULL, 0))
					{
						cs_addrs->nl->remove_shm = TRUE;
						sys$synch(efn_immed_wait, &iosb);
					}
				}
				rel_crit(gv_cur_region);
			} else if ((cancelled_timer && (0 > cs_addrs->nl->wcs_timers)) || cancelled_dbsync_timer)
			{	/* cancelled pending db or jnl flush timers - flush database and journal buffers to disk */
				grab_crit(gv_cur_region);
				/* we need to sync the epoch as the fact that there is no active pending flush timer implies
				 * there will be noone else who will flush the dirty buffers and EPOCH to disk in a timely fashion
				 */
				wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH | WCSFLU_SYNC_EPOCH);
				rel_crit(gv_cur_region);
			}
			if (JNL_ENABLED(cs_addrs->hdr)
				&& ((NOJNL != jpc->channel) && !JNL_FILE_SWITCHED(jpc)
                                || (we_are_last_writer
					&& (0 != memcmp(cs_addrs->nl->jnl_file.jnl_file_id.fid, zero_fid, SIZEOF(zero_fid))))))
			{	/* We need to close the journal file cleanly if we have the latest generation journal file open
				 *	or if we are the last writer and the journal file is open in shared memory (not necessarily
				 *	by ourselves e.g. the only process that opened the journal got shot abnormally)
				 * Note: we should not rely on the shared memory value of csa->nl->jnl_file.jnl_file_id
				 * 	if we are not the last writer as it can be concurrently updated.
				 */
				grab_crit(gv_cur_region);
				if (JNL_ENABLED(cs_addrs->hdr))
				{
					/* jnl_ensure_open/set_jnl/jnl_file_close/jnl_put_jrt_pini/pfin need it */
					SET_GBL_JREC_TIME;
					/* Before writing to jnlfile, adjust jgbl.gbl_jrec_time if needed to maintain time order
					 * of jnl records. This needs to be done BEFORE the jnl_ensure_open as that could write
					 * journal records (if it decides to switch to a new journal file).
					 */
					jbp = jpc->jnl_buff;
					ADJUST_GBL_JREC_TIME(jgbl, jbp);
					if (!we_are_last_writer || is_src_server)
					{	/* last writer of this database should actually execute the "else" part which calls
						 * set_jnl_file_close(), but if it happens to be the source server, then we should
						 * not go there because set_jnl_file_close() does gtm_enq and gtm_deq on jpc->jnllsb
						 * and the source-server is the only one that bypasses getting those locks in
						 * jnl_file_open. instead we call jnl_file_close() to just write an EOF record for
						 * us.
						 */
						jnl_status = jnl_ensure_open();
						if (0 == jnl_status)
						{
							if (!jgbl.mur_extract)
							{
								if (we_are_last_writer && (0 == jpc->pini_addr))
									jnl_put_jrt_pini(cs_addrs);
								if (0 != jpc->pini_addr)
									jnl_put_jrt_pfin(cs_addrs);
							}
							if (FALSE == nonclst_bg)
							{	/* in the case of MM, the above jnl_put_jrt_{pini,pfin}() calls
								 * would have started timers to do jnl-qio through the call to
								 * jnl_mm_timer() from jnl_write(). we do not want the timer routine
								 * jnl_mm_timer_write() to pop and try dereferencing csa->jnl or
								 * csa->nl after we have closed the journal or have detached from
								 * this region's shared memory. cancel those timers here. Unix does
								 * not suffer from this issue since over there, jnl_mm_timer_write()
								 * is not region-based and hence does the proper checks on each
								 * region before trying to dereference csa->jnl or csa->nl.
								 */
								sys$cantim(gv_cur_region, PSL$C_USER);
							}
							/* if not the last writer and no pending flush timer left,
							 * do jnl flush now.
							 */
							if (!we_are_last_writer && (0 > cs_addrs->nl->wcs_timers))
							{
								if (SS_NORMAL != (jnl_status = jnl_flush(gv_cur_region)))
								{
									send_msg(VARLSTCNT(9) ERR_JNLFLUSH, 2, JNL_LEN_STR(cs_data),
										ERR_TEXT, 2,
										RTS_ERROR_TEXT( \
										"Error with journal flush in gds_rundown2"),
										jnl_status);
									/* jnl file lost has been triggered */
									assert(NOJNL == jpc->channel);
									/* In this routine, all code that follows from here on does
									 * not assume anything about the journaling characteristics
									 * of this database so it is safe to continue execution
									 * even though journaling got closed in the middle.
									 */
								}
							}
							jnl_file_close(gv_cur_region, we_are_last_writer, FALSE);
						}
					} else
					{	/* since csa->nl->ref_cnt is not a reliable indicator of last writer status, we
						 * want to make sure any other process having this journal file open relinquishes
						 * control of it even though "we_are_last_writer" is TRUE. hence the call to
						 * set_jnl_file_close().  We do not check the return value below as it is not clear
						 * what to do in case of error.
						 */
						status = set_jnl_file_close(SET_JNL_FILE_CLOSE_RUNDOWN);
						assert(SS$_NORMAL == status);
					}
					assert(jpc->jnllsb->lockid || is_src_server || (NOJNL == jpc->channel));
				}
				rel_crit(gv_cur_region);
			}
		}
		/* release the journal file lock if we have a non-zero jnllsb->lockid */
		if ((NULL != jpc) && (0 != jpc->jnllsb->lockid))
		{
			assert(read_write);
			status = gtm_deq(jpc->jnllsb->lockid, NULL, PSL$C_USER, 0);
			assert((SS$_NORMAL == status)
				|| (SS$_IVLOCKID == status)
					&& gtm_white_box_test_case_enabled);
			jpc->jnllsb->lockid = 0;
		}
	}
	/* Get the remove_shm status from node_local before deleting address space of global section.
	 * If cs_addrs->nl->donotflush_dbjnl is TRUE, it means we can safely remove shared memory without compromising data
	 * 	integrity as a reissue of recover will restore the database to a consistent state.
	 */
	remove_shm = !vermismatch && (cs_addrs->nl->remove_shm || cs_addrs->nl->donotflush_dbjnl);
	/* -------------------------- take care of address space ------------------------- */
	if (FALSE == nonclst_bg)
	{
		cs_addrs->lock_addrs[1] = (sm_uc_ptr_t)(cs_addrs->nl) + ROUND_UP(LOCK_SPACE_SIZE(cs_addrs->hdr)
						+ NODE_LOCAL_SPACE(cs_addrs->hdr) + JNL_SHARE_SIZE(cs_addrs->hdr)
						+ SHMPOOL_BUFFER_SIZE + 1, OS_PAGE_SIZE) + 1;
		cs_addrs->lock_addrs[0] = (sm_uc_ptr_t)(cs_addrs->nl);
		gtm_deltva(cs_addrs->lock_addrs, retadr, PSL$C_USER);
	}
	outaddrs[0] = cs_addrs->db_addrs[0] - OS_PAGE_SIZE;	/* header no access page */
	outaddrs[1] = cs_addrs->db_addrs[1] + OS_PAGE_SIZE;	/* trailer no access page */
	gtm_deltva(outaddrs, retadr, PSL$C_USER);
	/* -------------------------- take care of global section ------------------------- */
	assert (!is_rcvr_server);
	ipc_deleted = FALSE;
	/* Don't dismantle global section if last writer didn't succeed in flushing the cache out */
	if (SS$_NORMAL == last_one_status)
	{
		if (remove_shm)
		{
			global_name("GT$S", &FILE_INFO(gv_cur_region)->file_id, name_buff);
			desc.dsc$w_length = name_buff[0];
			desc.dsc$b_dtype = DSC$K_DTYPE_T;
			desc.dsc$b_class = DSC$K_CLASS_S;
			desc.dsc$a_pointer = &name_buff[1];
			assert(!vermismatch);
			del_sec(SEC$M_SYSGBL, &desc, NULL);
			if (FALSE == nonclst_bg)
			{
				name_buff[4] = 'L';
				del_sec(SEC$M_SYSGBL, &desc, NULL);
			}
			ipc_deleted = TRUE;
		} else if (is_src_server || is_updproc)
		{
			gtm_putmsg(VARLSTCNT(6) ERR_DBRNDWNWRN, 4, DB_LEN_STR(gv_cur_region), process_id, process_id);
			send_msg(VARLSTCNT(6) ERR_DBRNDWNWRN, 4, DB_LEN_STR(gv_cur_region), process_id, process_id);
		} else
			send_msg(VARLSTCNT(6) ERR_DBRNDWNWRN, 4, DB_LEN_STR(gv_cur_region), process_id, process_id);
	}
	/* ------------------------- take care of locks ------------------------------------ */
	if (TRUE == clustered)
	{
		/* Dequeue locks after delete section in ccp_close,
		   acquire lock before create section in gvcst_init,
		   release lock after delete section in gds_rundown */
		status = gtm_deq(gds_info->cx_cntl_lsb.lockid, NULL, PSL$C_USER, 0);
		assert(SS$_NORMAL == status);
		gds_info->cx_cntl_lsb.lockid = 0;
		status = gtm_deq(gds_info->file_cntl_lsb.lockid, NULL, PSL$C_USER, 0);
		assert(SS$_NORMAL == status);
		gds_info->file_cntl_lsb.lockid = 0;
		ccp_sendmsg(CCTR_CLOSE, &FILE_INFO(gv_cur_region)->file_id);
	} else
	{
		if (SS$_NORMAL == last_one_status)
		{
			gds_info->file_cntl_lsb.valblk[0] = 0;
			gtm_enqw(EFN$C_ENF, LCK$K_NLMODE, &gds_info->file_cntl_lsb, LCK$M_CONVERT | LCK$M_VALBLK,
				NULL, 0, NULL, 0, NULL, PSL$C_USER, 0);
		}
		status = gtm_deq(gds_info->cx_cntl_lsb.lockid, NULL, PSL$C_USER, 0);
		assert(SS$_NORMAL == status);
		gds_info->cx_cntl_lsb.lockid = 0;
		status = gtm_deq(gds_info->file_cntl_lsb.lockid, NULL, PSL$C_USER, 0);
		assert(SS$_NORMAL == status);
		gds_info->file_cntl_lsb.lockid = 0;
	}
	if (!ipc_deleted)
	{
		GET_CUR_TIME;
		if (is_src_server)
			gtm_putmsg(VARLSTCNT(8) ERR_IPCNOTDEL, 6, CTIME_BEFORE_NL, time_ptr,
				LEN_AND_LIT("Source server"), REG_LEN_STR(gv_cur_region));
		if (is_updproc)
			gtm_putmsg(VARLSTCNT(8) ERR_IPCNOTDEL, 6, CTIME_BEFORE_NL, time_ptr,
				LEN_AND_LIT("Update process"), REG_LEN_STR(gv_cur_region));
		if (mupip_jnl_recover)
		{
			gtm_putmsg(VARLSTCNT(8) ERR_IPCNOTDEL, 6, CTIME_BEFORE_NL, time_ptr,
				LEN_AND_LIT("Mupip journal process"), REG_LEN_STR(gv_cur_region));
			send_msg(VARLSTCNT(8) ERR_IPCNOTDEL, 6, CTIME_BEFORE_NL, time_ptr,
				LEN_AND_LIT("Mupip journal process"), REG_LEN_STR(gv_cur_region));
		}
	}
	REVERT;
	/* -------------------------- nullify pointers ------------------------------------- */
	sys$dassgn(gds_info->fab->fab$l_stv);
	gv_cur_region->open = FALSE;
	cs_addrs->hdr = NULL;
	cs_addrs->nl = NULL;
	REMOVE_CSA_FROM_CSADDRSLIST(cs_addrs);	/* remove "cs_addrs" from list of open regions (cs_addrs_list) */
	cs_addrs->db_addrs[0] = NULL;
	cs_addrs->lock_addrs[0] = NULL;
}
