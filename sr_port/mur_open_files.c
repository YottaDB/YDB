/****************************************************************
 *								*
 * Copyright (c) 2003-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stdlib.h"
#include "min_max.h"
#include "cli.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "gdscc.h"			/* needed for tp.h */
#include "gdskill.h"			/* needed for tp.h */
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "hashtab.h"
#include "muprec.h"
#include "io.h"
#include "iosp.h"
#include "dpgbldir.h"
#include "have_crit.h"
#include "repl_sem.h"
#include "ftok_sems.h"
#include "repl_instance.h"
#include "mu_rndwn_repl_instance.h"
#include "deferred_signal_handler.h"
#include "mu_rndwn_file.h"
#include "read_db_files_from_gld.h"
#include "mur_db_files_from_jnllist.h"
#include "gtmmsg.h"
#include "file_head_read.h"
#include "mupip_exit.h"
#include "mu_gv_cur_reg_init.h"
#include "dbfilop.h"
#include "mur_read_file.h"	/* for "mur_fread_eof" prototype */
#include "tp_change_reg.h"
#include "gds_rundown.h"
#include "is_file_identical.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "mu_rndwn_replpool.h"
#include "gtm_logicals.h"
#include <sys/sem.h>
#include "tp.h"			/* for "insert_region" prototype */
#include "gtm_time.h"
#include "interlock.h"
#include "eintr_wrapper_semop.h"
#include "gtm_semutils.h"
#include "sleep_cnt.h"
#include "gdsbgtr.h"
#include "gtmsource_srv_latch.h"
#include "do_semop.h"
#include "wcs_sleep.h"
#include "wcs_flu.h"
#include "wcs_recover.h"
#include "is_proc_alive.h"
#include "anticipatory_freeze.h"

#define RELEASE_ACCESS_CONTROL(REGLIST)												\
{																\
	unix_db_info		*lcl_udi;											\
	gd_region		*lcl_reg;											\
	reg_ctl_list		*lcl_rctl;											\
	int			save_errno;											\
																\
	lcl_reg = REGLIST->reg;													\
	lcl_rctl = REGLIST->rctl;												\
	lcl_udi = FILE_INFO(lcl_reg);												\
	assert(INVALID_SEMID != lcl_udi->semid);										\
	assert(lcl_udi->grabbed_access_sem && lcl_rctl->standalone);								\
	if (0 != (save_errno = do_semop(lcl_udi->semid, DB_CONTROL_SEM, -1, SEM_UNDO)))						\
	{															\
		assert(FALSE);	/* we hold it, so we should be able to release it*/						\
		rts_error_csa(CSA_ARG(REG2CSA(lcl_reg)) VARLSTCNT(12) ERR_CRITSEMFAIL, 2, DB_LEN_STR(lcl_reg), ERR_SYSCALL, 5,	\
				RTS_ERROR_LITERAL("semop()"), CALLFROM, save_errno);						\
	}															\
	lcl_udi->grabbed_access_sem = FALSE;											\
	lcl_rctl->standalone = FALSE;												\
}

#define GRAB_ACCESS_CONTROL(REGLIST)											\
{															\
	unix_db_info		*lcl_udi;										\
	gd_region		*lcl_reg;										\
	reg_ctl_list		*lcl_rctl;										\
	int			save_errno, sopcnt, status;								\
	struct sembuf		sop[3];											\
															\
	SET_GTM_SOP_ARRAY(sop, sopcnt, FALSE, SEM_UNDO);								\
	assert(2 == sopcnt);												\
	lcl_reg = REGLIST->reg;												\
	lcl_rctl = REGLIST->rctl;											\
	lcl_udi = FILE_INFO(lcl_reg);											\
	assert(INVALID_SEMID != lcl_udi->semid);									\
	if (lcl_udi->grabbed_access_sem)										\
		assert(lcl_rctl->standalone);										\
	else														\
	{														\
		SEMOP(lcl_udi->semid, sop, sopcnt, status, NO_WAIT);							\
		if (0 != status)											\
		{													\
			save_errno = errno;										\
			assert(FALSE);											\
			rts_error_csa(CSA_ARG(REG2CSA(lcl_reg)) VARLSTCNT(12) ERR_CRITSEMFAIL, 2, DB_LEN_STR(lcl_reg),	\
					ERR_SYSCALL, 5, RTS_ERROR_LITERAL("semop()"), CALLFROM, save_errno);		\
		}													\
		lcl_udi->grabbed_access_sem = TRUE;									\
		lcl_rctl->standalone = TRUE;										\
	}														\
}

GBLREF	gd_addr			*gd_header;
GBLREF	gd_region		*gv_cur_region;
GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF	mur_opt_struct		mur_options;
GBLREF 	mur_gbls_t		murgbl;
GBLREF	reg_ctl_list		*mur_ctl;
GBLREF	sgmnt_data		*cs_data;
GBLREF	boolean_t		jnlpool_init_needed;
GBLREF	boolean_t		holds_sem[NUM_SEM_SETS][NUM_SRC_SEMS];
GBLREF	int4			strm_index;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	uint4			process_id;

error_def(ERR_CRITSEMFAIL);
error_def(ERR_DBFILOPERR);
error_def(ERR_DBFRZRESETFL);
error_def(ERR_DBFRZRESETSUC);
error_def(ERR_DBJNLNOTMATCH);
error_def(ERR_DBRDONLY);
error_def(ERR_FILENOTFND);
error_def(ERR_FILEPARSE);
error_def(ERR_JNLBADRECFMT);
error_def(ERR_JNLDBTNNOMATCH);
error_def(ERR_JNLDBSEQNOMATCH);
error_def(ERR_JNLFILEDUP);
error_def(ERR_JNLFILEOPNERR);
error_def(ERR_JNLNMBKNOTPRCD);
error_def(ERR_JNLSTATEOFF);
error_def(ERR_JNLTNOUTOFSEQ);
error_def(ERR_MUKILLIP);
error_def(ERR_MUPCLIERR);
error_def(ERR_MUPJNLINTERRUPT);
error_def(ERR_MUSTANDALONE);
error_def(ERR_NOPREVLINK);
error_def(ERR_NOSTARFILE);
error_def(ERR_NOTALLJNLEN);
error_def(ERR_NOTALLREPLON);
error_def(ERR_ORLBKFRZOVER);
error_def(ERR_ORLBKFRZPROG);
error_def(ERR_ORLBKNOV4BLK);
error_def(ERR_ORLBKSTART);
error_def(ERR_REPLSTATEOFF);
error_def(ERR_RLBKNOBIMG);
error_def(ERR_ROLLBKINTERRUPT);
error_def(ERR_STARFILE);
error_def(ERR_SYSCALL);
error_def(ERR_TEXT);
error_def(ERR_WCBLOCKED);

#define		STAR_QUOTE "\"*\""

boolean_t mur_open_files()
{
	boolean_t			interrupted_rollback;
	int                             jnl_total, jnlno, regno, max_reg_total, errcode;
	unsigned int			full_len;
	unsigned short			jnl_file_list_len; /* cli_get_str requires a short */
	char                            jnl_file_list[MAX_LINE];
	char				*cptr, *cptr_last, *ctop;
	jnl_ctl_list                    *jctl, *temp_jctl;
	reg_ctl_list			*rctl, *rctl_top, tmp_rctl;
	gld_dbname_list			*gld_db_files, *curr;
	boolean_t                       star_specified, outofsync;
	redirect_list			*rl_ptr;
	replpool_identifier		replpool_id;
	sgmnt_data_ptr_t		csd;
	sgmnt_addrs			*csa;
	file_control			*fc;
	freeze_status			reg_frz_status;
	intrpt_state_t			prev_intrpt_state;
	onln_rlbk_reg_list		*reglist = NULL, *rl, *rl_last, *save_rl, *rl_new;
	boolean_t			x_lock, wait_for_kip, replinst_file_corrupt = FALSE, inst_requires_rlbk;
	boolean_t			jnlpool_sem_created;
	sgmnt_addrs			*tmpcsa;
	sgmnt_data			*tmpcsd;
	gd_region			*reg;
	int4				llcnt, max_epoch_interval = 0, idx;
	int				save_errno;
	unix_db_info			*udi;
	char				time_str[CTIME_BEFORE_NL + 2]; /* for GET_CUR_TIME macro */
	const char			*verbose_ptr;
	gtmsource_local_ptr_t		gtmsourcelocal_ptr;
	DEBUG_ONLY(int			semval;)
	DEBUG_ONLY(jnl_buffer_ptr_t	jb;)
	boolean_t			recov_interrupted;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	jnlpool_init_needed = !mur_options.update;
	jnl_file_list_len = MAX_LINE;
	if (FALSE == CLI_GET_STR_ALL("FILE", jnl_file_list, &jnl_file_list_len))
		mupip_exit(ERR_MUPCLIERR);
	if ((1 == jnl_file_list_len && '*' == jnl_file_list[0]) ||
		(STR_LIT_LEN(STAR_QUOTE) == jnl_file_list_len &&
		0 == memcmp(jnl_file_list, STAR_QUOTE, STR_LIT_LEN(STAR_QUOTE))))
	{
		star_specified = TRUE;
		if (NULL != mur_options.redirect)
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_STARFILE, 2, LEN_AND_LIT("REDIRECT qualifier"));
			mupip_exit(ERR_MUPCLIERR);
		}
	} else
	{
		star_specified = FALSE;
		if (mur_options.rollback && !mur_options.forward)
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_NOSTARFILE, 2, LEN_AND_LIT("ROLLBACK -BACKWARD qualifier"));
			mupip_exit(ERR_MUPCLIERR);
		}
	}
	/* We assume recovery will be done only on current global directory.
	 * That is, journal file names specified must be from current global directory.
	 */
	if (star_specified || mur_options.update)
	{	/* "*" is specified or it is -recover or -rollback. We require gtmgbldir to be set in all these cases */
		assert(NULL == gd_header);
		gvinit();	/* read in current global directory */
		assert(NULL != gd_header);
	}
	if (star_specified)
	{
		max_reg_total = gd_header->n_regions;
		gld_db_files = read_db_files_from_gld(gd_header);
	} else
		gld_db_files = mur_db_files_from_jnllist(jnl_file_list, jnl_file_list_len, &max_reg_total);
	if (NULL == gld_db_files)
		return FALSE;
	mur_ctl = (reg_ctl_list *)malloc(SIZEOF(reg_ctl_list) * max_reg_total);
	memset(mur_ctl, 0, SIZEOF(reg_ctl_list) * max_reg_total);
	curr = gld_db_files;
	murgbl.max_extr_record_length = DEFAULT_EXTR_BUFSIZE;
	murgbl.repl_standalone = FALSE;
	if (mur_options.rollback && !mur_options.forward)
	{	/* Rundown the Jnlpool and Recvpool. Do it only for backward rollback. For forward rollback, we expect
		 * the jnlpool/recvpool/database to be rundown already. We do not not currently touch the replication
		 * instance file nor do we look at jnlpool/recvpool during forward rollback so we skip this step.
		 */
		if (!repl_inst_get_name((char *)replpool_id.instfilename, &full_len, SIZEOF(replpool_id.instfilename),
				issue_gtm_putmsg, NULL))
		{	/* appropriate gtm_putmsg would have already been issued by repl_inst_get_name */
			return FALSE;
		}
		assert((int)NUM_SRC_SEMS == (int)NUM_RECV_SEMS);
		ASSERT_DONOT_HOLD_REPLPOOL_SEMS;
		assert((NULL == jnlpool) || (NULL == jnlpool->repl_inst_filehdr));
		if (!mu_rndwn_repl_instance(&replpool_id, FALSE, TRUE, &jnlpool_sem_created))
			return FALSE;	/* mu_rndwn_repl_instance will have printed appropriate message in case of error */
		assert(jgbl.onlnrlbk || INST_FREEZE_ON_ERROR_POLICY || ((NULL == jnlpool) || (NULL == jnlpool->jnlpool_ctl)));
		ASSERT_HOLD_REPLPOOL_SEMS;
		assert((NULL != jnlpool) && (NULL != jnlpool->repl_inst_filehdr));
		assert(INVALID_SEMID != jnlpool->repl_inst_filehdr->jnlpool_semid);
		assert(INVALID_SEMID != jnlpool->repl_inst_filehdr->recvpool_semid);
		murgbl.repl_standalone = TRUE;
		/* Since rollback would not have necessarily done a jnlpool_init, it might not have initialized strm_index
		 * to the correct value in case of a supplementary instance. So do it now. This code is similar to that in
		 * jnlpool_init. Keep the two in sync (i.e. fix the other similarly if one changes).
		 */
		if (jnlpool->repl_inst_filehdr->is_supplementary)
		{
			assert((INVALID_SUPPL_STRM == strm_index) || (0 == strm_index));
			strm_index = 0;
		}
		ENABLE_FREEZE_ON_ERROR;
	}
	for (murgbl.reg_full_total = 0, rctl = mur_ctl, rctl_top = mur_ctl + max_reg_total;
										rctl < rctl_top; rctl++, curr = curr->next)
	{
		rctl->initialized = FALSE;
		rctl->gd = curr->gd;	/* region structure is already set with proper values */
		rctl->standalone = FALSE;
		rctl->csa = NULL;
		rctl->csd = NULL;
		rctl->jctl = rctl->jctl_head = rctl->jctl_alt_head = rctl->jctl_turn_around = rctl->jctl_apply_pblk = NULL;
		murgbl.reg_full_total++;	/* mur_close_files() expects rctl->csa and rctl->jctl to be initialized.
						 * so consider this rctl only after those have been initialized.
						 */
		/* Do region specific initialization */
		init_hashtab_mname(&rctl->gvntab, 0, HASHTAB_NO_COMPACT, HASHTAB_NO_SPARE_TABLE);	/* for mur_forward() */
		rctl->db_ctl = (file_control *)malloc(SIZEOF(file_control));
		memset(rctl->db_ctl, 0, SIZEOF(file_control));
		mur_rctl_desc_alloc(rctl); /* Allocate rctl->mur_desc associated buffers */
		/* For redirect we just need to change the name of database. recovery will redirect to new database file */
		if (mur_options.redirect)
		{
			for (rl_ptr = mur_options.redirect;  rl_ptr != NULL;  rl_ptr = rl_ptr->next)
			{
				if (curr->gd->dyn.addr->fname_len == rl_ptr->org_name_len &&
				    0 == memcmp(curr->gd->dyn.addr->fname, rl_ptr->org_name, rl_ptr->org_name_len))
				{
					curr->gd->dyn.addr->fname_len = rl_ptr->new_name_len;
					memcpy(curr->gd->dyn.addr->fname, rl_ptr->new_name, curr->gd->dyn.addr->fname_len);
					curr->gd->dyn.addr->fname[curr->gd->dyn.addr->fname_len] = 0;
					break;
				}
			}
		}
		if (!mupfndfil(rctl->gd, NULL, LOG_ERROR_FALSE))
		{
			rctl->db_present = FALSE;
			if (mur_options.update || star_specified)
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_FILENOTFND, 2, DB_LEN_STR(rctl->gd));
				return FALSE;
			}
		} else
		{
			rctl->db_present = TRUE;
			if (mur_options.update)
			{
				if (!jgbl.onlnrlbk)
				{
					if (!STANDALONE(rctl->gd))	/* STANDALONE macro calls mu_rndwn_file() */
					{
						gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_MUSTANDALONE, 2,
							       DB_LEN_STR(rctl->gd));
						return FALSE;
					}
					rctl->standalone = TRUE;
				}
			}
			if (mur_options.update || mur_options.extr[GOOD_TN])
			{
	        		gvcst_init(rctl->gd, NULL);
				TP_CHANGE_REG(rctl->gd);
				if (jgbl.onlnrlbk)
				{
					if (!cs_data->fully_upgraded)
					{
						gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(6) ERR_ORLBKNOV4BLK, 4,
							       REG_LEN_STR(gv_cur_region), DB_LEN_STR(gv_cur_region));
						return FALSE;
					}
					max_epoch_interval = MAX(cs_data->epoch_interval, max_epoch_interval);
					assert(!cs_addrs->hold_onto_crit);
					rctl->standalone = TRUE;
				}
				if (mur_options.update)
				{
					assert(rctl->standalone);
					assert((FILE_INFO(rctl->gd))->grabbed_access_sem);
					if (rctl->gd->read_only)
					{	/* recover/rollback cannot proceed if the process has read-only permissions */
						gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_DBRDONLY, 2,
							       DB_LEN_STR(rctl->gd));
						return FALSE;
					}
				}
			}
		}
		assert(!mur_options.update || rctl->standalone);
		assert(mur_options.update || !rctl->standalone);
	}
	assert(murgbl.reg_full_total == max_reg_total);
	DEBUG_ONLY(curr = gld_db_files;)
	assert(!jgbl.onlnrlbk || (0 != max_epoch_interval) || (NULL == jnlpool) || (NULL == jnlpool->jnlpool_ctl));
	if (jgbl.onlnrlbk)
	{
		inst_requires_rlbk = FALSE;
		udi = FILE_INFO(jnlpool->jnlpool_dummy_reg);
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_ORLBKSTART, 4,
			     LEN_AND_STR(jnlpool->repl_inst_filehdr->inst_info.this_instname), LEN_AND_STR(udi->fn));
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_ORLBKSTART, 4,
			       LEN_AND_STR(jnlpool->repl_inst_filehdr->inst_info.this_instname), LEN_AND_STR(udi->fn));
		/* Need to get the gtmsource_srv_latch BEFORE grab_crit to avoid deadlocks */
		if ((NULL != jnlpool) && (NULL != jnlpool->jnlpool_ctl))
		{
			assert(udi == FILE_INFO(jnlpool->jnlpool_dummy_reg));
			csa = &udi->s_addrs;
			ASSERT_VALID_JNLPOOL(csa);
			assert(INVALID_SEMID != jnlpool->repl_inst_filehdr->jnlpool_semid);
			assert(INVALID_SEMID != jnlpool->repl_inst_filehdr->recvpool_semid);
			assert(INVALID_SEMID != jnlpool->repl_inst_filehdr->jnlpool_shmid);
			gtmsourcelocal_ptr = &jnlpool->gtmsource_local_array[0];
			for (idx = 0; NUM_GTMSRC_LCL > idx; idx++, gtmsourcelocal_ptr++)
			{	/* Get hold of all the gtmsource_srv_latch in all the source server slots in the journal pool. Hold
				 * onto it until the end (in mur_close_files).
				 */
				jnlpool->gtmsource_local = gtmsourcelocal_ptr;
				if (!grab_gtmsource_srv_latch(&gtmsourcelocal_ptr->gtmsource_srv_latch,
						2 * max_reg_total * max_epoch_interval, GRAB_GTMSOURCE_SRV_LATCH_ONLY))
					assertpro(FALSE); /* should not reach here due to rts_error in the above function */

			}
		}
		/* For online rollback we need to grab crit on all the regions. But, this has to be done in the ftok order. To
		 * setup the regions in the ftok order invoke insert_region.  Note that all we need is a list of regions sorted by
		 * the inode/device numbers. So, a bubble sort would have worked just fine. But, since most of the code uses
		 * insert_region, we are using it here to avoid code duplication. Note: We could ideally use tp_reg_list here as
		 * well.
		 */
		for (rctl = mur_ctl, rctl_top = mur_ctl + max_reg_total; rctl < rctl_top; rctl++)
		{
			assert(rctl->gd->open); /* region should have been opened by now */
			rl_new = (onln_rlbk_reg_list *)insert_region(rctl->gd,
					(tp_region **)(&reglist), NULL, SIZEOF(onln_rlbk_reg_list));
			assert(NULL != rl_new);
			rl_new->rctl = rctl; /* store the backward link to rctl */
		}
		/* The following loop is mostly borrowed from tp_crit_all_regions() */
		TREF(donot_write_inctn_in_wcs_recover) = TRUE; /* donot write INCTN if wcs_recover is called below */
		TREF(wcs_recover_done) = FALSE;
		for (llcnt = 0; ; llcnt++)
		{
			x_lock = TRUE; /* assume success */
			for (rl = reglist; NULL != rl; rl = rl->fPtr)
			{
				reg = rl->reg;
				tmpcsa = &FILE_INFO(reg)->s_addrs;
				tmpcsd = tmpcsa->hdr;
				GRAB_ACCESS_CONTROL(rl);
				grab_crit(reg);
				DO_CHILLED_AUTORELEASE(tmpcsa, tmpcsd);
				if (FROZEN(tmpcsd))
				{
					save_rl = rl;
					rl = rl->fPtr;	/* Increment so we release the lock we actually got */
					x_lock = FALSE;
					break;
				}
			}
			if (x_lock)
				break;
			rl_last = rl;
			for (rl = reglist; rl_last != rl; rl = rl->fPtr)
			{
				rel_crit(rl->reg);
				RELEASE_ACCESS_CONTROL(rl);
			}
			assert((NULL != save_rl) && (tmpcsa->region == save_rl->reg));
			GET_CUR_TIME(time_str);
			send_msg_csa(CSA_ARG(REG2CSA(save_rl->reg)) VARLSTCNT(8) ERR_ORLBKFRZPROG, 6, CTIME_BEFORE_NL, time_str,
				     REG_LEN_STR(save_rl->reg), DB_LEN_STR(save_rl->reg));
			gtm_putmsg_csa(CSA_ARG(REG2CSA(save_rl->reg)) VARLSTCNT(8) ERR_ORLBKFRZPROG, 6, CTIME_BEFORE_NL, time_str,
				       REG_LEN_STR(save_rl->reg), DB_LEN_STR(save_rl->reg));
			while (FROZEN(tmpcsd))
			{
				if (MAXHARDCRITS < llcnt)
					wcs_sleep(llcnt); /* Don't waste CPU cycles anymore */
				llcnt++;
			}
			GET_CUR_TIME(time_str);
			send_msg_csa(CSA_ARG(REG2CSA(save_rl->reg)) VARLSTCNT(8) ERR_ORLBKFRZOVER, 6, CTIME_BEFORE_NL, time_str,
				     REG_LEN_STR(save_rl->reg), DB_LEN_STR(save_rl->reg));
			gtm_putmsg_csa(CSA_ARG(REG2CSA(save_rl->reg)) VARLSTCNT(8) ERR_ORLBKFRZOVER, 6, CTIME_BEFORE_NL, time_str,
				       REG_LEN_STR(save_rl->reg), DB_LEN_STR(save_rl->reg));
		}
		inst_requires_rlbk |= TREF(wcs_recover_done);
		assert(x_lock); /* Now we have crit on all the regions (for this global directory) */
		/* Get the replication locks. If there is NO journal pool, then there is nothing more to do as we had
		 * already flushed the journal pool in mu_rndwn_replpool. But, if the journal pool exists, then it indicates
		 * that processes are still attached to the journal pool. So, grab the journal pool locks, flush the journal
		 * pool and set the field in the journal pool indicating that online rollback is in progress and other
		 * processes need to back off.
		 */
		if ((NULL != jnlpool) && (NULL != jnlpool->jnlpool_ctl))
		{	/* Validate the journal pool is accessible and the offsets of various structures within it are intact */
			grab_lock(jnlpool->jnlpool_dummy_reg, TRUE, GRAB_LOCK_ONLY);
			csa->hold_onto_crit = TRUE;	/* No more unconditional rel_lock() */
			assert(jnlpool->repl_inst_filehdr->crash); /* since we haven't removed the journal pool */
			/* Wait for any pending phase2 commits in jnlpool to finish */
			while (jnlpool->jnlpool_ctl->phase2_commit_index1 != jnlpool->jnlpool_ctl->phase2_commit_index2)
			{
				repl_phase2_cleanup(jnlpool);
				if (jnlpool->jnlpool_ctl->phase2_commit_index1 == jnlpool->jnlpool_ctl->phase2_commit_index2)
					break;
				JPL_TRACE_PRO(jnlpool->jnlpool_ctl, jnl_pool_write_sleep);
				SLEEP_USEC(1, FALSE);
			}
			repl_inst_flush_jnlpool(FALSE, FALSE);
			assert((0 == jnlpool->jnlpool_ctl->onln_rlbk_pid)
				|| !is_proc_alive(jnlpool->jnlpool_ctl->onln_rlbk_pid, 0));
			jnlpool->jnlpool_ctl->onln_rlbk_pid = process_id;
		}
		replinst_file_corrupt = TRUE;
		/* Indicate to all other processes that ONLINE ROLLBACK is now in progress */
		for (rctl = mur_ctl, rctl_top = mur_ctl + max_reg_total; rctl < rctl_top; rctl++)
		{
			reg = rctl->gd;
			TP_CHANGE_REG(reg);
#			ifdef DEBUG
			udi = FILE_INFO(reg);
			assert(1 == (semval = semctl(udi->semid, DB_CONTROL_SEM, GETVAL)));
#			endif
			assert(cs_addrs->now_crit);
			cs_addrs->hold_onto_crit = TRUE; /* No more unconditional grab_crit/rel_crit on this region */
			/* Do wcs_flu which guarantees that all pending phase 2 commits are done and hardened to disk */
			/* If Online Rollback is invoked right after a crash in the midst of a commit, early_tn
			 * can be different from curr_tn (as curr_tn is not incremented until Phase 1 of the commit).
			 * A wcs_flu done below has asserts to the effect that it is never invoked with an out-of-sync
			 * early_tn and curr_tn. But, Online Rollback is an exception in that it will be holding crit
			 * for the entire duration and will be fixing the cache and rolling back the database to a
			 * consistent state. So, set the early_tn to curr_tn in case they differ. To avoid unnecessary
			 * CPU cycles, set it to curr_tn unconditionally.
			 */
			assert(1 >= (cs_addrs->ti->early_tn - cs_addrs->ti->curr_tn));
			inst_requires_rlbk |= (cs_addrs->ti->early_tn != cs_addrs->ti->curr_tn);
			cs_addrs->ti->early_tn = cs_addrs->ti->curr_tn;
			if (!wcs_flu(WCSFLU_NONE))
			{
				assert(cs_addrs->nl->wcs_phase2_commit_pidcnt); /* only reason why wcs_flu can fail */
				SET_TRACEABLE_VAR(cs_addrs->nl->wc_blocked, TRUE);
				BG_TRACE_PRO_ANY(cs_addrs, wc_blocked_onln_rlbk);
				send_msg_csa(CSA_ARG(cs_addrs) VARLSTCNT(8) ERR_WCBLOCKED, 6, LEN_AND_LIT("wc_blocked_onln_rlbk"),
					 process_id, &cs_addrs->ti->curr_tn, DB_LEN_STR(gv_cur_region));
				inst_requires_rlbk = TRUE;
				assert(TREF(donot_write_inctn_in_wcs_recover)); /* should still be set to TRUE */
				wcs_recover(reg);
				/* Now that wcs_recover is done, do a wcs_flu(WCSFLU_NONE) once again. This time we don't expect
				 * wcs_flu to error out.
				 */
				assertpro(wcs_flu(WCSFLU_NONE));
			}
			assert(0 == cs_addrs->nl->wcs_phase2_commit_pidcnt); /* should be zero after wcs_flu and wcs_recover */
			/* we played with access control lock before, make sure we hold it on all regions now */
			assert(rctl->standalone);
			assert((FILE_INFO(reg))->grabbed_access_sem);
			assert((0 == cs_addrs->nl->onln_rlbk_pid) || !is_proc_alive(cs_addrs->nl->onln_rlbk_pid, 0));
			cs_addrs->nl->onln_rlbk_pid = process_id;
#			ifdef DEBUG
			/* ensure that we don't have any pending journal writes or flushes */
			assert(!JNL_ALLOWED(cs_data) || (NULL != cs_addrs->jnl) && (NULL != cs_addrs->jnl->jnl_buff));
			if (NULL != cs_addrs->jnl)
			{
				jb = cs_addrs->jnl->jnl_buff;
				assert((jb->freeaddr == jb->dskaddr) && (jb->dskaddr == jb->fsync_dskaddr)
						&& (jb->rsrv_freeaddr == jb->rsrv_freeaddr));
			}
#			endif
			if (cs_data->kill_in_prog)
				gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(6) ERR_MUKILLIP, 4, DB_LEN_STR(reg),
					       LEN_AND_LIT("ONLINE ROLLBACK"));
			/* Ensure that inhibit_kills is ZERO at this point. This is because, we hold crit at this point and anyone
			 * who wants to set inhibit_kills need crit. The reason this is important is because t_end/tp_tend has
			 * logic to restart if inhibit_kills is set to TRUE and we don't want online rollback to restart
			 */
			assert(0 == cs_addrs->nl->inhibit_kills);
		}
		TREF(donot_write_inctn_in_wcs_recover) = FALSE;
	}
	for (rctl = mur_ctl, rctl_top = mur_ctl + max_reg_total; rctl < rctl_top; rctl++)
	{
		if (rctl->db_present)
		{
			if (mur_options.update || mur_options.extr[GOOD_TN])
			{	/* NOTE: Only for collation info extract needs database access */
				DEFER_INTERRUPTS(INTRPT_IN_MUR_OPEN_FILES, prev_intrpt_state);	/* temporarily disable
												 * MUPIP STOP/signal handling. */
				TP_CHANGE_REG(rctl->gd);
				udi = FILE_INFO(rctl->gd);
				csa = rctl->csa = &udi->s_addrs;
				csd = rctl->csd = rctl->csa->hdr;
				assert(!jgbl.onlnrlbk || (csa->now_crit && csa->hold_onto_crit));
				if (mur_options.update)
				{
					assert(!csa->nl->donotflush_dbjnl || jgbl.onlnrlbk);
					csa->nl->donotflush_dbjnl = TRUE; /* indicate gds_rundown/mu_rndwn_file to not wcs_flu()
									   * this shared memory until recover/rlbk cleanly exits */
				}
				assert(!JNL_ENABLED(csd) || 0 == csd->jnl_file_name[csd->jnl_file_len]);
				rctl->db_ctl->file_info = FILE_CNTL(rctl->gd)->file_info;
				rctl->recov_interrupted = csd->recov_interrupted;
				if (mur_options.update && rctl->recov_interrupted)
				{	/* interrupted recovery might have changed current csd's jnl_state/repl_state.
					 * restore the state of csd before the start of interrupted recovery.
					 */
					if (mur_options.forward)
					{	/* error out. need fresh backup of database for forward recovery */
						gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_MUPJNLINTERRUPT, 2,
							       DB_LEN_STR(rctl->gd));
						ENABLE_INTERRUPTS(INTRPT_IN_MUR_OPEN_FILES, prev_intrpt_state);
						return FALSE;
					}
					/* In case rollback with non-zero resync_seqno got interrupted, we would have
					 * written intrpt_recov_resync_seqno. If so, cannot use RECOVER now.
					 * In all other cases, intrpt_recov_resolve_time would have been written.
					 */
					if (!mur_options.rollback)
					{
						interrupted_rollback = FALSE;
						if (csd->intrpt_recov_resync_seqno)
							interrupted_rollback = TRUE;
						else
						{
							for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)
							{
								if (csd->intrpt_recov_resync_strm_seqno[idx])
								{
									interrupted_rollback = TRUE;
									break;
								}
							}
						}
						if (interrupted_rollback)
						{
							gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_ROLLBKINTERRUPT, 2,
								       DB_LEN_STR(rctl->gd));
							ENABLE_INTERRUPTS(INTRPT_IN_MUR_OPEN_FILES, prev_intrpt_state);
							return FALSE;
						}
					}
					csd->jnl_state = csd->intrpt_recov_jnl_state;
					csd->repl_state = csd->intrpt_recov_repl_state;
				}
				/* Save current states */
				rctl->jnl_state = csd->jnl_state;
				rctl->repl_state = csd->repl_state;
				rctl->before_image = csd->jnl_before_image;
				rctl->initialized = TRUE;
				ENABLE_INTERRUPTS(INTRPT_IN_MUR_OPEN_FILES, prev_intrpt_state); /* reenable the interrupts */
				if (mur_options.update)
				{
					if (!mur_options.rollback)
					{
						if (!mur_options.forward && !JNL_ENABLED(csd))
						{
							if (!star_specified)
							{
								gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_JNLSTATEOFF, 2,
									       DB_LEN_STR(rctl->gd));
								return FALSE;
							}
							continue;
						}
					} else if (!mur_options.forward)
					{	/* MUPIP JOURNAL -ROLLBACK -BACKWARD */
						if (!REPL_ENABLED(csd))
						{	/* Replication is either OFF or WAS_ON. Journaling could be ENABLED or not.
							 * If replication is OFF and journaling is DISABLED, there is no issue.
							 * Any other combination (including replication being WAS_ON) is an error
							 * as we don't have the complete set of journal records to do the rollback.
							 */
							if (REPL_ALLOWED(csd) || JNL_ENABLED(csd))
							{
								gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_REPLSTATEOFF, 2,
									       DB_LEN_STR(rctl->gd));
								return FALSE;
							}
							continue;
						} else if (!rctl->before_image)
						{	/* Replicated database with NOBEFORE_IMAGE journaling.
							 * ROLLBACK is allowed only if -FETCHRESYNC or -RESYNC is specified.
							 */
							if (!mur_options.fetchresync_port && !mur_options.resync_specified)
							{
								gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(4)
										ERR_RLBKNOBIMG, 2, DB_LEN_STR(rctl->gd));
								return FALSE;
							}
							mur_options.rollback_losttnonly = TRUE;
							/* Since we won't be touching the journal pool/file there is no question
							 * of leaving things in a corrupt state.
							 */
							replinst_file_corrupt = FALSE;
						}
					} else
					{	/* MUPIP JOURNAL -ROLLBACK -FORWARD */
						if (!JNL_ENABLED(csd))
							continue; /* this region is not journaled so no rollback needed here */
						/* Note: We allow NOBEFORE and BEFORE image journaling for -ROLLBACK -FORWARD */
						/* Note: We allow replication to be enabled for -ROLLBACK -FORWARD. It is disabled
						 * 	 below.
						 */
					}
					if (FROZEN(csd) && !jgbl.onlnrlbk)
					{	/* region_freeze should release freeze here. For ONLINE ROLLBACK we would have
						 * waited for the freeze to be lifted off before
						 */
						reg_frz_status = region_freeze(rctl->gd, FALSE, TRUE, FALSE, FALSE, FALSE);
						assert (!FROZEN(rctl->csa->hdr));
						assert(REG_FREEZE_SUCCESS == reg_frz_status);
						if (REG_ALREADY_FROZEN == reg_frz_status)
						{
							gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_DBFRZRESETFL, 2,
								       DB_LEN_STR(rctl->gd));
							return FALSE;
						}
						gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_DBFRZRESETSUC, 2,
							       DB_LEN_STR(rctl->gd));
					}
					/* save current jnl/repl state before changing in case recovery is interrupted */
					csd->intrpt_recov_jnl_state = csd->jnl_state;
					csd->intrpt_recov_repl_state = csd->repl_state;
					csd->recov_interrupted = TRUE;
					/* Temporarily change current state. mur_close_files() will restore them as appropriate.
					 * Note though that for -RECOVER -FORWARD or -ROLLBACK -FORWARD, jnl state is not restored.
					 */
					if (mur_options.forward && JNL_ENABLED(csd))
						csd->jnl_state = jnl_closed;
					csd->repl_state = repl_closed;
					csd->file_corrupt = TRUE;
					/* flush the changed csd to disk */
					fc = FILE_CNTL(rctl->gd);
					fc->op = FC_WRITE;
					/* Note: csd points to shared memory and is already aligned
					 * appropriately even if db was opened using O_DIRECT.
					 */
					fc->op_buff = (sm_uc_ptr_t)csd;
					fc->op_len = SGMNT_HDR_LEN;
					fc->op_pos = 1;
					dbfilop(fc);
				}
				rctl->db_tn = csd->trans_hist.curr_tn;
				/* Some routines use csa */
				csa->jnl_state= csd->jnl_state;
				csa->repl_state = csd->repl_state;
				csa->jnl_before_image = csd->jnl_before_image;
			} else
			{	/* temporarily disable MUPIP STOP/signal handling. */
				DEFER_INTERRUPTS(INTRPT_IN_MUR_OPEN_FILES, prev_intrpt_state);
				/* NOTE: csa field is NULL, if we do not open database */
				csd = rctl->csd = (sgmnt_data_ptr_t)malloc(SGMNT_HDR_LEN);
				assert(0 == rctl->gd->dyn.addr->fname[rctl->gd->dyn.addr->fname_len]);
				/* 1) show 2) extract 3) verify action does not need standalone access.
				 * In this case csa is NULL
				 */
				if (!file_head_read((char *)rctl->gd->dyn.addr->fname, rctl->csd, SGMNT_HDR_LEN))
				{
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_DBFILOPERR, 2, REG_LEN_STR(rctl->gd));
					return FALSE;
				}
				rctl->jnl_state = csd->jnl_state;
				rctl->repl_state = csd->repl_state;
				rctl->before_image = csd->jnl_before_image;
				rctl->initialized = TRUE;
				ENABLE_INTERRUPTS(INTRPT_IN_MUR_OPEN_FILES, prev_intrpt_state); /* reenable the interrupts */
			}
			/* For star_specified we open journal files here.
			 * For star_specified we cannot do anything if journaling is disabled
			 */
			if (star_specified && JNL_ALLOWED(csd))
			{
				jctl = (jnl_ctl_list *)malloc(SIZEOF(jnl_ctl_list));
				memset(jctl, 0, SIZEOF(jnl_ctl_list));
				rctl->jctl_head = rctl->jctl = jctl;
				jctl->jnl_fn_len = csd->jnl_file_len;
				memcpy(jctl->jnl_fn, csd->jnl_file_name, csd->jnl_file_len);
				jctl->jnl_fn[jctl->jnl_fn_len] = 0;
				/* If system crashed during rename, following will fix it .
				 * Following function is directly related to cre_jnl_file_common
				 */
				if (rctl->standalone || (rctl->csa && rctl->csa->now_crit))
					cre_jnl_file_intrpt_rename(rctl->csa);
				if (SS_NORMAL != mur_fopen(jctl, rctl))
					return FALSE;	/* mur_fopen() would have done the appropriate gtm_putmsg() */
				if (SS_NORMAL != (jctl->status = mur_fread_eof(jctl, rctl)))
				{
					gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(9) ERR_JNLBADRECFMT, 3, jctl->jnl_fn_len,
						       jctl->jnl_fn, jctl->rec_offset, ERR_TEXT, 2,
						       LEN_AND_LIT("mur_fread_eof failed"));
					return FALSE;
				}
				assert((csa == rctl->csa) || !mur_options.update);
				assert(!jgbl.onlnrlbk || (csa == &udi->s_addrs));
				if (jgbl.onlnrlbk && jctl->jfh->crash
						&& !udi->shm_created && !jctl->jfh->recover_interrupted && !inst_requires_rlbk)
				{	/* If the journal file is crashed, mur_fread_eof invokes mur_fread_eof_crash to read the
					 * last valid record from the journal file and marks the journal file as NOT properly
					 * closed. However, for online rollback, if the shared memory exists and the journal file
					 * crashed, we can still consider it as a no-crash scenario as long as the following
					 * conditions are met:
					 * 1. journal file is not created by a previously interrupted recovery
					 * 2. wcs_recover did NOT encounter a DBDANGER situation
					 */
					jctl->properly_closed = TRUE;
				}
				if (mur_options.verbose)
				{
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUINFOSTR, 4,
						       LEN_AND_LIT("Module : mur_open_files"),
						       LEN_AND_LIT("Post mur_fread_eof details"));
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUINFOSTR, 4, LEN_AND_LIT("    Journal file"),
							JNL_LEN_STR(rctl->csd));
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUINFOUINT4, 4,
						       LEN_AND_LIT("    Last valid record offset"),
						       jctl->lvrec_off, jctl->lvrec_off);
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUINFOUINT4, 4,
						       LEN_AND_LIT("    Last valid record time"), jctl->lvrec_time,
						       jctl->lvrec_time);
					verbose_ptr = jctl->tail_analysis ? "TRUE" : "FALSE";
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUINFOSTR, 4,
						       LEN_AND_LIT("      Tail analysis done"), LEN_AND_STR(verbose_ptr));
					verbose_ptr = jctl->properly_closed ? "TRUE" : "FALSE";
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUINFOSTR, 4,
						       LEN_AND_LIT("      Properly closed"), LEN_AND_STR(verbose_ptr));
				}
				if (!is_file_identical((char *)jctl->jfh->data_file_name, (char *)rctl->gd->dyn.addr->fname))
				{
					gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_DBJNLNOTMATCH, 6, DB_LEN_STR(rctl->gd),
						jctl->jnl_fn_len, jctl->jnl_fn,
						jctl->jfh->data_file_name_length, jctl->jfh->data_file_name);
					return FALSE;
				}
			}
		} /* End rctl->db_present */
	} /* End for */
	if (jgbl.mur_rollback && !mur_options.forward)
		jnlpool->repl_inst_filehdr->file_corrupt = replinst_file_corrupt;
	/* At this point mur_ctl[] has been created from the current global directory database file names
	 * or from the journal file header's database names.
	 * For star_specified == TRUE implicitly only current generation journal files are specified and already opened
	 * For star_specified == FALSE user can specify multiple generations. We need to sort them
	 */
	if (!star_specified)
	{
		jnlno = 0;
		cptr = jnl_file_list;
		ctop = &jnl_file_list[jnl_file_list_len];
		while (cptr < ctop)
		{
			jctl = (jnl_ctl_list *)malloc(SIZEOF(jnl_ctl_list));
			memset(jctl, 0, SIZEOF(jnl_ctl_list));
			cptr_last = cptr;
			while (0 != *cptr && ',' != *cptr && '"' != *cptr &&  ' ' != *cptr)
				++cptr;
			if (!get_full_path(cptr_last, (unsigned int)(cptr - cptr_last),
						(char *)jctl->jnl_fn, &jctl->jnl_fn_len, MAX_FN_LEN, &jctl->status2))
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_FILEPARSE, 2, cptr - cptr_last, cptr_last,
					       jctl->status2);
				return FALSE;
			}
			cptr++;	/* skip separator */
			if (SS_NORMAL != mur_fopen(jctl, NULL))	/* don't know rctl yet */
				return FALSE;	/* mur_fopen() would have done the appropriate gtm_putmsg() */
			for (rctl = mur_ctl, rctl_top = mur_ctl + murgbl.reg_full_total; rctl < rctl_top; rctl++)
			{
				if (rctl->gd->dyn.addr->fname_len == jctl->jfh->data_file_name_length &&
						(0 == memcmp(jctl->jfh->data_file_name, rctl->gd->dyn.addr->fname,
								rctl->gd->dyn.addr->fname_len)))
					break;
			}
			if (rctl == rctl_top)
			{
				for (rl_ptr = mur_options.redirect;  (NULL != rl_ptr);  rl_ptr = rl_ptr->next)
				{
					if ((jctl->jfh->data_file_name_length == rl_ptr->org_name_len)
							&& (0 == memcmp(jctl->jfh->data_file_name,
									rl_ptr->org_name, rl_ptr->org_name_len)))
						break;
				}
				if (NULL != rl_ptr)
				{
					for (rctl = mur_ctl, rctl_top = mur_ctl + murgbl.reg_full_total; rctl < rctl_top; rctl++)
					{
						if (rctl->gd->dyn.addr->fname_len == rl_ptr->new_name_len &&
								(0 == memcmp(rctl->gd->dyn.addr->fname, rl_ptr->new_name,
										rl_ptr->new_name_len)))
							break;
					}
				}
				/* db list was created from journal file header. So it is not possible */
				assertpro(rctl != rctl_top);
			}
			/* Detect and report 1st case of any duplicated files in mupip forward recovery command. */
			if (mur_options.forward)
			{
				if (SS_NORMAL == (save_errno = filename_to_id(&jctl->fid, (char *)jctl->jnl_fn)))
				{	/* WARNING: assignment above */
					for (temp_jctl = rctl->jctl_head; temp_jctl; temp_jctl = temp_jctl->next_gen)
					{
						if (is_gdid_identical(&jctl->fid, &temp_jctl->fid))
						{
							gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(6) ERR_JNLFILEDUP, 4,
								       jctl->jnl_fn_len, jctl->jnl_fn, temp_jctl->jnl_fn_len,
								       temp_jctl->jnl_fn);
							return FALSE;
						}
					}
				} else
				{
					gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(11) ERR_JNLFILEOPNERR, 2, jctl->jnl_fn_len,
						       jctl->jnl_fn, ERR_SYSCALL, 5, LEN_AND_LIT("fstat"), CALLFROM, save_errno);
					return FALSE;
				}
			}
			if (SS_NORMAL != (jctl->status = mur_fread_eof(jctl, rctl)))
			{
				gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(5) ERR_JNLBADRECFMT, 3, jctl->jnl_fn_len,
					       jctl->jnl_fn, jctl->rec_offset);
				return FALSE;
			}
			/* Now, we have found the region for this jctl */
			csd = rctl->csd;
			if (!mur_options.forward)
			{
				rctl->jctl = rctl->jctl_head = jctl;
				if (mur_options.update && !is_file_identical((char *)csd->jnl_file_name, (char *)jctl->jnl_fn))
				{
					gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(8) ERR_JNLNMBKNOTPRCD, 6, jctl->jnl_fn_len,
						       jctl->jnl_fn, JNL_LEN_STR(csd), DB_LEN_STR(rctl->gd));
					return FALSE;
				}
			} else
			{	/* rctl->jctl_head will have the lowest bov_tn of the journals of the region
				 * Then next_gen items will be non-decreasing order of bov_tn */
				if (NULL == rctl->jctl_head)
				{
					assert(NULL == rctl->jctl);
					rctl->jctl = rctl->jctl_head = jctl;
				} else
				{
					temp_jctl = jctl;
					jctl = rctl->jctl_head;
					if (temp_jctl->jfh->bov_tn < jctl->jfh->bov_tn ||
						(temp_jctl->jfh->bov_tn == jctl->jfh->bov_tn &&
						temp_jctl->jfh->eov_tn < jctl->jfh->eov_tn))
					{
						assert(NULL == jctl->prev_gen);
						temp_jctl->prev_gen = NULL;
						temp_jctl->next_gen = jctl;
						jctl->prev_gen = temp_jctl;
						rctl->jctl_head = temp_jctl;
					} else
					{
						while (NULL != jctl->next_gen &&
							((jctl->next_gen->jfh->bov_tn < temp_jctl->jfh->bov_tn) ||
							(jctl->next_gen->jfh->bov_tn == temp_jctl->jfh->bov_tn &&
							jctl->next_gen->jfh->eov_tn < temp_jctl->jfh->eov_tn)))
							jctl = jctl->next_gen ;
						temp_jctl->next_gen = jctl->next_gen;
						temp_jctl->prev_gen = jctl;
						if (NULL != jctl->next_gen)
							jctl->next_gen->prev_gen = temp_jctl;
						jctl->next_gen = temp_jctl;
					}
			       }
			}   /* mur_options.forward */
		} /* for jnlno */
	}
	/* If not all regions of a global directory are processed, we shrink mur_ctl array and conserve space.
	 * It is specially needed for later code
	 */
	murgbl.reg_total = murgbl.reg_full_total;
	for (regno = 0; regno < murgbl.reg_total; regno++)
	{
		if (NULL == mur_ctl[regno].jctl)
		{
			for (--murgbl.reg_total; murgbl.reg_total > regno; murgbl.reg_total--)
			{
				rctl = &mur_ctl[murgbl.reg_total];
				if (NULL != (jctl = rctl->jctl_head))
				{
					assert(jctl == rctl->jctl); /* rctl->jctl and rctl->jctl_head should be same now */
					tmp_rctl = mur_ctl[regno];
					mur_ctl[regno] = *rctl;
					*rctl = tmp_rctl;
					rctl = &mur_ctl[regno];
					MUR_FIX_JCTL_BACK_POINTER_TO_RCTL(jctl, rctl, &mur_ctl[murgbl.reg_total], TRUE);
					break;
				}
			}
		}
	}
	assert(murgbl.reg_full_total == max_reg_total);
	if (murgbl.reg_total < murgbl.reg_full_total)
	{
		errcode = (!mur_options.rollback ? ERR_NOTALLJNLEN : ERR_NOTALLREPLON);
		if (0 == murgbl.reg_total)
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT (4) MAKE_MSG_ERROR(errcode), 2, LEN_AND_LIT("all"));
		else
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT (4) errcode, 2, LEN_AND_LIT("one or more"));
	}
	if (0 == murgbl.reg_total)
		return FALSE;
	/* From this point consider only regions with journals to be processed (murgbl.reg_total)
	 * However mur_close_files will close all regions opened (murgbl.reg_full_total)
	 */
	for (rctl = mur_ctl, rctl_top = mur_ctl + murgbl.reg_total; rctl < rctl_top; rctl++)
	{
		jctl = rctl->jctl_head;
		if (rctl->db_present && (mur_options.update || mur_options.extr[GOOD_TN]))
			rctl->csa->miscptr = (void *)rctl; /* needed by gdsfilext_nojnl/jnl_write_pini/mur_pini_addr_reset */
		if (mur_options.update)
		{
			csd = rctl->csd;
			if (mur_options.chain && mur_options.forward)
			{	/* User might have not specified journal file starting tn matching database curr_tn.
				 * So try to open previous generation journal files and add to linked list */
				rctl->jctl = jctl;	/* asserted by mur_insert_prev */
				while ((jctl->jfh->bov_tn > csd->trans_hist.curr_tn)
						|| (mur_options.rollback && (jctl->jfh->start_seqno > csd->reg_seqno)))
				{
					if (0 == jctl->jfh->prev_jnl_file_name_length)
					{
						if (jctl->jfh->bov_tn > csd->trans_hist.curr_tn)
							gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(11) ERR_JNLDBTNNOMATCH, 9,
							       jctl->jnl_fn_len, jctl->jnl_fn, LEN_AND_LIT("beginning"),
							       &jctl->jfh->bov_tn, DB_LEN_STR(rctl->gd), &csd->trans_hist.curr_tn,
							       &csd->jnl_eovtn);
						else
							gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(8) ERR_JNLDBSEQNOMATCH, 6,
							       jctl->jnl_fn_len, jctl->jnl_fn,
							       &jctl->jfh->start_seqno, DB_LEN_STR(rctl->gd), &csd->reg_seqno);
						gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(4) ERR_NOPREVLINK, 2,
								jctl->jnl_fn_len, jctl->jnl_fn);
						return FALSE;
					} else if (!mur_insert_prev(&jctl))
						return FALSE;
				}
			}
			if (mur_options.forward)
			{
				assert(!mur_options.rollback || !mur_options.notncheck); /* -ROLLBACK -FORWARD does not support
											  * -NOCHECKTN. Asserted above. */
				if (!mur_options.notncheck)
				{
					if (jctl->jfh->bov_tn != csd->trans_hist.curr_tn)
					{
						gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(11) ERR_JNLDBTNNOMATCH, 9,
							       jctl->jnl_fn_len, jctl->jnl_fn, LEN_AND_LIT("beginning"),
							       &jctl->jfh->bov_tn, DB_LEN_STR(rctl->gd),
							       &csd->trans_hist.curr_tn, &csd->jnl_eovtn);
						return FALSE;
					}
					if (mur_options.rollback && (jctl->jfh->start_seqno > csd->reg_seqno))
					{
						gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(8) ERR_JNLDBSEQNOMATCH, 6,
							       jctl->jnl_fn_len, jctl->jnl_fn,
							       &jctl->jfh->start_seqno, DB_LEN_STR(rctl->gd), &csd->reg_seqno);
						return FALSE;
					}
				}
			} else /* Backward Recovery */
			{
				if (jctl->jfh->eov_tn != csd->trans_hist.curr_tn)
				{
					recov_interrupted = rctl->recov_interrupted || csd->intrpt_recov_resync_seqno
								|| csd->intrpt_recov_tp_resolve_time;
					/* 'outofsync' variable identifies situations in which backward recovery
					 * proceeds if inequality (csd->jnl_eovtn <= jfh->eov_tn <= csd->curr_tn) is TRUE.
					 * So if,
					 * i)   backward recovery is interrupted at any time except at turn around point just after
					 *      database header is synched OR
					 *ii)   database is crashed but journaling is behind database i.e.
					 *	jfh->eov_tn < csd->trans_hist.curr_tn
					 *	outofsync is set to TRUE.
                                         * backward recovery does not proceed if,
					 *  i)  database is crashed and journaling is ahead of database
					 * ii)  database is cleanly terminated and outofsync is FALSE
					 *iii)  outofsync is TRUE but above mentioned inequality is FALSE and
					 *      interruption or crash did not occur while processing turn around point
					 */
					outofsync = (recov_interrupted ||
					  	     (jctl->jfh->crash && (jctl->jfh->eov_tn < csd->trans_hist.curr_tn)));
					if ((jctl->jfh->crash && (jctl->jfh->eov_tn > csd->trans_hist.curr_tn) &&
						!recov_interrupted) || (!jctl->jfh->crash && !outofsync) ||
						(outofsync && !csd->turn_around_point && (csd->jnl_eovtn != csd->trans_hist.curr_tn)
						  && (csd->jnl_eovtn > jctl->jfh->eov_tn)))
					{
						gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(11) ERR_JNLDBTNNOMATCH, 9,
							       jctl->jnl_fn_len, jctl->jnl_fn, LEN_AND_LIT("end"),
							       &jctl->jfh->eov_tn, DB_LEN_STR(rctl->gd), &csd->trans_hist.curr_tn,
							       &csd->jnl_eovtn);
						return FALSE;
					}
				}
			}
		} /* if mur_options.update */
		if (mur_options.extr[GOOD_TN])
		{
			csa = rctl->csa;
			if (NULL != csa)
			{
				if (csa->nl->wc_blocked)
					TREF(donot_write_inctn_in_wcs_recover) = TRUE;
			}
		}
		while (NULL != jctl->next_gen) /* Check for continuity */
		{
			if (!mur_options.notncheck && (jctl->next_gen->jfh->bov_tn != jctl->jfh->eov_tn))
			{
				gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(8) ERR_JNLTNOUTOFSEQ, 6,
					       &jctl->jfh->eov_tn, jctl->jnl_fn_len, jctl->jnl_fn,
					       &jctl->next_gen->jfh->bov_tn, jctl->next_gen->jnl_fn_len, jctl->next_gen->jnl_fn);
				return FALSE;
			}
			jctl = jctl->next_gen;
		}
		rctl->jctl = jctl;      /* latest in linked list */
	} /* end for */
	return TRUE;
}
