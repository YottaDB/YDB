/****************************************************************
 *								*
 * Copyright (c) 2005-2024 Fidelity National Information	*
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

#include "gdsroot.h"
#include "gdsblk.h"
#include "gdsbml.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "v6_gdsfhead.h"
#include "gdsblkops.h"
#include "filestruct.h"
#include "error.h"
#include "gdscc.h"
#include "gdskill.h"
#include "jnl.h"
#include "cli.h"
#include "iosp.h"		/* for SS_NORMAL */

/* Prototypes */
#include "change_reg.h"
#include "cws_insert.h"		/* for cw_stagnate hashtab operations */
#include "do_semop.h"
#include "format_targ_key.h"	/* for ISSUE_GVSUBOFLOW_ERROR */
#include "gds_blk_upgrade.h"
#include "gds_rundown.h"
#include "gtmmsg.h"		/* for gtm_putmsg prototype */
#include "gtm_semutils.h"
#include "gvcst_protos.h"
#include "gvt_inline.h"
#include "is_proc_alive.h"
#include "muextr.h"
#include "mu_getlst.h"
#include "mu_gv_cur_reg_init.h"
#include "mu_reorg.h"
#include "mu_reorg_upgrd_dwngrd.h"
#include "mu_updwn_ver_inline.h"
#include "mu_rndwn_file.h"
#include "mu_upgrade_bmm.h"
#include "mu_upgrd_dngrd_confirmed.h"
#include "mupip_exit.h"
#include "mupip_reorg.h"
#include "mupip_upgrade.h"
#include "send_msg.h"		/* for send_msg */
#include "sleep_cnt.h"
#include "t_abort.h"
#include "t_begin.h"
#include "t_end.h"
#include "t_qread.h"
#include "t_retry.h"
#include "t_write.h"
#include "targ_alloc.h"
#include "util.h"		/* for util_out_print prototype */
#include "verify_db_format_change_request.h"
#include "wcs_flu.h"
#include "wcs_phase2_commit_wait.h"
#include "wcs_sleep.h"

#include "repl_msg.h"		/* for gtmsource.h */
#include "gtmsource.h"		/* for jnlpool_addrs_ptr_t */

#define	MAX_TRIES	600	/* arbitrary value */
#define	MAX_BLK_TRIES	10	/* Times to repeatedly try a GVT root */

#define IS_ONLNRLBK_ACTIVE(CSA)	(0 != CSA->nl->onln_rlbk_pid)


LITREF	char			gtm_release_name[];
LITREF	int4			gtm_release_name_len;

GBLREF	bool			error_mupip, mu_ctrlc_occurred;
GBLREF	boolean_t		debug_mupip;
GBLREF	boolean_t		mu_reorg_more_tries, mu_reorg_process, need_kip_incr;
GBLREF	boolean_t		mu_reorg_nosafejnl;			/* TRUE if NOSAFEJNL explicitly specified */
GBLREF	char			*update_array, *update_array_ptr;	/* for the BLK_INIT/BLK_SEG/BLK_ADDR macros */
GBLREF	cw_set_element		cw_set[];
GBLREF	gd_addr			*gd_header;
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_key			*gv_altkey, *gv_currkey, *gv_currkey_next_reorg;
GBLREF	gv_namehead		*gv_target, *gv_target_list, *reorg_gv_target, *upgrade_gv_target;
GBLREF	hash_table_int8		cw_stagnate;				/* Release blocks added via CWS_INSERT - see mu_swap_blk */
GBLREF	inctn_detail_t		inctn_detail;				/* holds detail to fill in to inctn jnl record */
GBLREF	inctn_opcode_t		inctn_opcode;
GBLREF	int4			gv_keysize;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	tp_region		*grlist;
GBLREF	trans_num		mu_reorg_upgrd_dwngrd_blktn;	/* tn in blkhdr of current block processed by REORG UP/DOWNGRADE */
GBLREF	trans_num		start_tn;
GBLREF	uint4			mu_upgrade_in_prog;			/* 2 if MUPIP REORG UPGRADE/DOWNGRADE is in progress */
GBLREF	uint4			update_array_size, update_trans, process_id;
GBLREF	unsigned char		rdfail_detail, t_fail_hist[CDB_MAX_TRIES];
GBLREF 	unsigned int		t_tries;

#define V6TOV7UPGRADEABLE 1

error_def(ERR_COMMITWAITSTUCK);
error_def(ERR_DBRNDWN);
#ifndef V6TOV7UPGRADEABLE	/* Reject until upgrades are enabled */
error_def(ERR_GTMCURUNSUPP);
#endif
error_def(ERR_MUNOACTION);
error_def(ERR_MUNODBNAME);
error_def(ERR_MUNOFINISH);
error_def(ERR_MUNOUPGRD);
error_def(ERR_MUPGRDSUCC);
error_def(ERR_MUQUALINCOMP);
error_def(ERR_MUSTANDALONE);
error_def(ERR_MUUPGRDNRDY);
error_def(ERR_REORGCTRLY);
error_def(ERR_REORGUPCNFLCT);
error_def(ERR_WCBLOCKED);

STATICDEF gtm_int8		gv_trees, tot_data_blks, tot_dt, tot_levl_cnt, tot_splt_cnt;
STATICDEF block_id		index_blks_at_v7;

/******************************************************************************************
 * Called by mupip_reorg with a -upgrade qualifier. It calls mu_upgrd_dngrd_confirmed,
 * gets a set of regions/segments and for each invokes verify_db_format_change_request and
 * then region and then uses find_gvt_roots to work through the dir tree
 *
 * Input Parameters:
 *	none
 * Output Parameters:
 *	none
 ******************************************************************************************/
void	mu_reorg_upgrd_dwngrd(void)
{
	block_id		curr_blk;
	block_id		last_blks_to_upgrd, max_blks_to_upgrd;
	boolean_t		error, file, is_bg, region;
	cache_rec_ptr_t		child_cr = NULL;
	char			errtext[OUT_BUFF_SIZE];
	char			*wcblocked_ptr;
	gd_region		*reg;
	gd_segment		*seg;
	inctn_opcode_t		save_inctn_opcode;
	int4			lcnt, resetcnt, sleepcnt, status;
	jnl_private_control	*jpc;
	jnl_buffer_ptr_t	jbp;
	mname_entry		gvname;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	trans_num		curr_tn;
	tp_region		*rptr, single;
	uint4			jnl_status;
	unsigned char		gname[SIZEOF(mident_fixed) + 2];

#ifndef V6TOV7UPGRADEABLE	/* Reject until upgrades are enabled */
	mupip_exit(ERR_GTMCURUNSUPP);
#endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Structure checks */
	assert((8192) == SIZEOF(sgmnt_data));						/* Verify V7 file header hasn't changed */
	assert((8192) == SIZEOF(v6_sgmnt_data));					/* Verify V6 file header hasn't changed */
	error = FALSE;
	/* Get list of regions to upgrade */
	file = (CLI_PRESENT == cli_present("FILE"));
	region = (CLI_PRESENT == cli_present("REGION")) || (CLI_PRESENT == cli_present("R"));
	gvinit();									/* initialize gd_header needed mu_getlst */
	if ((file == region) && (TRUE == file))
		mupip_exit(ERR_MUQUALINCOMP);
	else if (region)
	{
		mu_getlst("REG_NAME", SIZEOF(tp_region));
		rptr = grlist;	/* setup of grlist down implicitly by insert_region() called in mu_getlst() */
		if (error_mupip)
		{
			util_out_print("!/MUPIP REORG -UPGRADE cannot proceed with above errors!/", TRUE);
			mupip_exit(ERR_MUNOACTION);
		}
	} else if (file)
	{	/* WARNING: REORG does not current take a -FILE qualifier */
		mu_gv_cur_reg_init();
		seg = gv_cur_region->dyn.addr;
		seg->fname_len = MAX_FN_LEN;
		if (!cli_get_str("FILE", (char *)&seg->fname[0], &seg->fname_len))
			mupip_exit(ERR_MUNODBNAME);
		seg->fname[seg->fname_len] = '\0';
		rptr = &single;		/* a dummy value that permits one trip through the loop */
		rptr->fPtr = NULL;
		rptr->reg = gv_cur_region;
		gd_header = gv_cur_region->owning_gd;
		insert_region(gv_cur_region, &(grlist), NULL, SIZEOF(tp_region));
	}
	if (!mu_upgrd_dngrd_confirmed())
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TEXT, 2, LEN_AND_LIT("Upgrade canceled by user"));
		mupip_exit(ERR_MUNOACTION);
	}
	TREF(skip_file_corrupt_check) = TRUE;				/* Prevent concurrent ONLINE ROLLBACK causing DBFLCORRP */
	upgrade_gv_target = targ_alloc(MAX_KEY_SZ, NULL, NULL);		/* UPGRADE needs space for gen_hist_for_blk */
	upgrade_gv_target->hist.depth = 0;
	upgrade_gv_target->alt_hist->depth = 0;
	reorg_gv_target = targ_alloc(MAX_KEY_SZ, NULL, NULL);		/* because it may be needed for before image journaling */
	reorg_gv_target->hist.depth = 0;
	reorg_gv_target->alt_hist->depth = 0;
	for (rptr = grlist; !mu_ctrlc_occurred && (NULL != rptr); rptr = rptr->fPtr)
	{	/* Iterate over regions again to upgrade gvt indices */
		reg = rptr->reg;
		gv_cur_region = reg;
		tot_data_blks = gv_trees = tot_dt = tot_levl_cnt = tot_splt_cnt = 0;
		sleepcnt = SLEEP_ONE_MIN;
		/* Override name map so that all names map to the current region */
		remap_globals_to_one_region(gd_header, gv_cur_region);
		if (NULL == gv_currkey_next_reorg)			/* Multi-region upgrade in progress, don't reallocate */
			GVKEY_INIT(gv_currkey_next_reorg, gv_keysize);
		else
			gv_currkey_next_reorg->end = 0;
		assert(DBKEYSIZE(MAX_KEY_SZ) == gv_keysize);		/* gv_keysize was init'ed by gvinit() in the caller */
		gvcst_init(reg, NULL);
		change_reg();
		csd = cs_data;
		csa = cs_addrs;
		is_bg = (dba_bg == csd->acc_meth);
		start_tn = csd->trans_hist.curr_tn;			/* When switching regions, pickup the curr_tn as start_tn */
		status = verify_db_format_change_request(reg, GDSV7m, "MUPIP REORG -UPGRADE");
		if (SS_NORMAL != status)
		{
			error = TRUE;					/* move on to the next region */
			if (csa->now_crit)
				rel_crit(reg);
			continue;
		}
		if (IS_ONLNRLBK_ACTIVE(csa))
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_REORGUPCNFLCT, 5,
					LEN_AND_LIT("REORG -UPGRADE"),
					LEN_AND_LIT("MUPIP ROLLBACK -ONLINE in progress"),
					csa->nl->onln_rlbk_pid);
			mupip_exit(ERR_MUNOFINISH);
		}
		while ((0 != csa->nl->trunc_pid) && (is_proc_alive(csa->nl->trunc_pid, 0)))
		{	/* Wait for a concurrent REORG -TRUNCATE to exit */
			if (SLEEP_ONE_MIN == sleepcnt)
				util_out_print("!/Region !AD : MUPIP REORG -TRUNCATE of !AD in progress, waiting for completion",
						TRUE, REG_LEN_STR(reg), DB_LEN_STR(reg));
			wcs_sleep(sleepcnt--);
			if (0 == sleepcnt)
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_REORGUPCNFLCT, 5,
						LEN_AND_LIT("REORG -UPGRADE"),
						LEN_AND_LIT("MUPIP REORG -TRUNCATE in progress"),
						csa->nl->trunc_pid);
				assert(FALSE);	/* REORG -TRUNCATE should have exited */
				mupip_exit(ERR_MUNOFINISH);
			}
		}
		if ((0 != csa->nl->reorg_upgrade_pid) && (is_proc_alive(csa->nl->reorg_upgrade_pid, 0)))
		{
			util_out_print("!/Region !AD : MUPIP REORG -UPGRADE of !AD in progress, skipping",
					TRUE, REG_LEN_STR(reg), DB_LEN_STR(reg));
			continue;
		}
		mu_upgrade_in_prog = MUPIP_REORG_UPGRADE_IN_PROGRESS;
		mu_reorg_more_tries = TRUE;
		csa->nl->reorg_upgrade_pid = process_id;
		util_out_print("!/Region !AD : MUPIP REORG -UPGRADE of !AD started (!UL of !UL)", TRUE,
					REG_LEN_STR(reg), DB_LEN_STR(reg), csd->blks_to_upgrd, csd->trans_hist.total_blks);
		gvname.var_name.addr = (char *)gname;
		gv_target = targ_alloc(csa->hdr->max_key_size, &gvname, reg);
		gv_target->root = DIR_ROOT;
		gv_target->clue.end = 0;
		curr_blk = DIR_ROOT;			/* DIR_ROOT to variable makes pointer to pass below */
		resetcnt = 0;
		max_blks_to_upgrd = csd->blks_to_upgrd;	/* Remaining blocks to determine when to stop */
		lcnt = MAX_TRIES;
		last_blks_to_upgrd = -1;		/* Ensure one retry */
		do
		{	/* always start or restart with DIR_ROOT */
			index_blks_at_v7 = 0;
			status = find_gvt_roots(&curr_blk, reg, &child_cr);
			if (is_bg && (NULL != child_cr))
			{	/* Release the cache record, transitively making the corresponding buffer, that this function just
				 * modified, avaiable for re-use. Doing so ensures that all the CRs being touched as part of the
				 * REORG UPGRADE do not accumulate creating a situation where "when everything is special, nothing
				 * is special" resulting in parent blocks being moved around in memory which causes restarts.
				 */
				child_cr->refer = FALSE;
			}
			/* Release crit and other cleanup - Only the GVT conversion loop should retain crit */
			t_abort(gv_cur_region, csa);
			assert(0 <= status);
			if (debug_mupip)
				util_out_print("ROOT:0x!@XQ\tstatus:!UL\tRemaining:!UL(!UL)\tIndex:!UL DT:!UL", TRUE, &curr_blk,
						status, csd->blks_to_upgrd, last_blks_to_upgrd, index_blks_at_v7, tot_dt);
			if (mu_ctrlc_occurred || IS_ONLNRLBK_ACTIVE(csa))
				break;
			if (cdb_sc_normal == status)
			{
				if (0 == csd->blks_to_upgrd)
					break;	/* Returned cleanly from descent and no more blocks to upgrade */
				else if (last_blks_to_upgrd == csd->blks_to_upgrd)
					break;	/* No change in blocks to upgrade count */
				/* After a good status return, stash the blocks to upgrade count. If it is the same on the next
				 * pass, then all done */
				last_blks_to_upgrd = csd->blks_to_upgrd;
			}
			if (resetcnt >= MAX_TRIES)
				break;	/* Concurrent updaters that increase the blocks to upgrade count can starve REORG online */
			if (max_blks_to_upgrd < csd->blks_to_upgrd)
			{	/* Should the blocks to upgrade count increase during the upgrade, reset the max tries count */
				max_blks_to_upgrd = csd->blks_to_upgrd;
				lcnt = MAX_TRIES;
				resetcnt++;
			} else if ((0 == --lcnt) || (resetcnt >= MAX_TRIES))
			{
#				ifdef DEBUG_BLKS_TO_UPGRD
				DEBUG_ONLY(gtm_fork_n_core());
#				endif
				break;
			}
		} while (TRUE);
		if (tot_data_blks)
			tot_data_blks -= gv_trees;
		if (0 > tot_data_blks)	/* Do not let this go below zero */
			tot_data_blks = 0;
		if (debug_mupip)
			util_out_print("ROOT:0x!@XQ\tstatus:!UL\tRemaining:!UL(!UL)\tIndex:!UL DT:!UL CTRLC:!UL",
					TRUE, &curr_blk, status, csd->blks_to_upgrd, last_blks_to_upgrd,
					index_blks_at_v7, tot_dt, mu_ctrlc_occurred);
		util_out_print("Region !AD : Index block upgrade !ADcomplete",
				TRUE, REG_LEN_STR(reg), mu_ctrlc_occurred ? 2 : 0, "in");
		util_out_print("Region !AD : Upgraded !@UQ index blocks for !@UQ global variable trees, ",
				FALSE, REG_LEN_STR(reg), &tot_dt, &gv_trees);
		util_out_print("splitting !@UQ block!AD, adding !@UQ directory tree level!AD",
				TRUE, &tot_splt_cnt, (1 == tot_splt_cnt) ? 0 : 1, "s",
				&tot_levl_cnt, (1 == tot_levl_cnt) ? 0 : 1, "s");
		util_out_print("Region !AD : Identified !@UQ associated data blocks",
				TRUE, REG_LEN_STR(reg), &tot_data_blks);
		if (mu_ctrlc_occurred || IS_ONLNRLBK_ACTIVE(csa))
		{
			mu_upgrade_in_prog = MUPIP_UPGRADE_OFF;
			mu_reorg_more_tries = FALSE;
			if (IS_ONLNRLBK_ACTIVE(csa))
				util_out_print("Region !AD : Ended due to concurrent ROLLBACK -ONLINE", TRUE);
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_MUUPGRDNRDY, 5, DB_LEN_STR(reg),
					RTS_ERROR_LITERAL("V7"), &csd->blks_to_upgrd);
			break;
		}
		if ((cdb_sc_normal == status) && ((0 == csd->blks_to_upgrd) || (-1 != last_blks_to_upgrd)))
		{	/* Normal status AND either blks_to_upgrd is zero OR this is the second pass */
			grab_crit(reg, WS_9);			/* Grab crit and update the file header in crit */
			/* Wait for concurrent phase2 commits to complete before switching the desired db format */
			if (csa->nl->wcs_phase2_commit_pidcnt && !wcs_phase2_commit_wait(csa, NULL))
			{	/* Set wc_blocked so next process to get crit will trigger cache-recovery */
				SET_TRACEABLE_VAR(csa->nl->wc_blocked, WC_BLOCK_RECOVER);
				wcblocked_ptr = WCS_PHASE2_COMMIT_WAIT_LIT;
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_WCBLOCKED, 6, LEN_AND_STR(wcblocked_ptr),
					process_id, &csd->trans_hist.curr_tn, DB_LEN_STR(reg));
				gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(7) ERR_COMMITWAITSTUCK, 5, process_id,
						1, csa->nl->wcs_phase2_commit_pidcnt, DB_LEN_STR(reg));
				error = TRUE;
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_MUUPGRDNRDY, 5, DB_LEN_STR(reg),
						RTS_ERROR_LITERAL("V7m"), &csd->blks_to_upgrd);
				rel_crit(reg);
				continue;
			}
			if (JNL_ENABLED(csd))
			{
				SET_GBL_JREC_TIME;	/* needed for jnl_ensure_open, jnl_write_pini and jnl_write_aimg_rec */
				jpc = csa->jnl;
				jbp = jpc->jnl_buff;
				/* Before writing to jnlfile, adjust jgbl.gbl_jrec_time if needed maintaining time order.
				 * This needs to be done BEFORE the jnl_ensure_open as that could write journal records
				 * (if it decides to switch to a new journal file)
				 */
				ADJUST_GBL_JREC_TIME(jgbl, jbp);
				jnl_status = jnl_ensure_open(reg, csa);
				if (SS_NORMAL == jnl_status)
				{
					save_inctn_opcode = inctn_opcode;
					inctn_opcode = inctn_db_format_change;
					inctn_detail.blks2upgrd_struct.blks_to_upgrd_delta = csd->blks_to_upgrd;
					if (0 == jpc->pini_addr)
						jnl_write_pini(csa);
					jnl_write_inctn_rec(csa);
					inctn_opcode = save_inctn_opcode;
				} else
				{
					gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(csd), DB_LEN_STR(reg));
					error = TRUE;
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_MUUPGRDNRDY, 5, DB_LEN_STR(reg),
							RTS_ERROR_LITERAL("V7m"), &csd->blks_to_upgrd);
					rel_crit(reg);
					continue;
				}
			}
			curr_tn = csd->trans_hist.curr_tn;
			if ((csd->fully_upgraded = (0 == csd->blks_to_upgrd)))	/* Upgrade complete, WARNING assignment */
				csd->offset = 0;		/* Reset offset disabling V6p upgrades on read */
			csd->minor_dbver = GDSMVCURR;		/* Raise the DB minor version to current */
			csd->max_tn = MAX_TN_V7;		/* Expand TN limit */
			SET_TN_WARN(csd, csd->max_tn_warn);	/* if max_tn changed above, max_tn_warn also needs the same */
			assert(curr_tn < csd->max_tn);		/* ensure CHECK_TN macro below will not issue TNTOOLARGE */
			CHECK_TN(csa, csd, curr_tn);		/* can issue rts_error TNTOOLARGE */
			csd->trans_hist.early_tn = csd->trans_hist.curr_tn + 1;
			INCREMENT_CURR_TN(csd);			/* Increment TN and flush the header with crit */
			wcs_flu(WCSFLU_IN_COMMIT | WCSFLU_FLUSH_HDR | WCSFLU_CLEAN_DBSYNC);
			rel_crit(reg);				/* release crit */
			if (csd->fully_upgraded)
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_MUPGRDSUCC, 6, DB_LEN_STR(reg),
						RTS_ERROR_LITERAL("upgraded"), gtm_release_name_len, gtm_release_name);
			else
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_MUPGRDSUCC, 6, DB_LEN_STR(reg),
						RTS_ERROR_LITERAL("upgraded index blocks"), gtm_release_name_len, gtm_release_name);
		} else
		{
			error = TRUE;
			snprintf(errtext, sizeof(errtext), "Iteration:%d, Status Code:%d",
						1 + (-1 != last_blks_to_upgrd), (unsigned char)status);
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_MUUPGRDNRDY, 5, DB_LEN_STR(reg),
					RTS_ERROR_LITERAL("V7"), &csd->blks_to_upgrd);
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TEXT, 2, LEN_AND_STR(errtext));
		}
		error |= gds_rundown(CLEANUP_UDI_TRUE);		/* Rundown region and cleanup */
		mu_upgrade_in_prog = MUPIP_UPGRADE_OFF;
		mu_reorg_more_tries = FALSE;
	}
	if (error || mu_ctrlc_occurred)
	{
		if (mu_ctrlc_occurred)
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_REORGCTRLY);
		status = ERR_MUNOFINISH;
	} else
		status = SS_NORMAL;
	mupip_exit(status);
	return;
}

/******************************************************************************************
 * This recursively traverses the directory tree using upgrade_idx_block to upgrade each gvt
 *
 * Input Parameters:
 * 	curr_blk points to a block in the directory tree to search
 * 	gv_cur_region points to the base structure for the region
 * Output Parameters:
 * 	*cr points to the cache record that this function was working on for the caller to release
 * 	(enum_cdb_sc) returns cdb_sc_normal which the code expects or a retry code
 ******************************************************************************************/
enum cdb_sc find_gvt_roots(block_id *curr_blk, gd_region *reg, cache_rec_ptr_t *cr)
{
	block_id	blk_pter, blocks_left;
	boolean_t	is_bg, is_mm, was_crit;
	cache_rec	curr_blk_cr;
	cache_rec_ptr_t	child_cr = NULL;
	enum db_ver	blk_ver;
	gvnh_reg_t	*gvnh_reg;
	gtm_int8	lcl_gv_trees;
	int		key_cmpc, key_len, level, new_blk_sz, num_recs, rec_sz, split_blks_added, split_levels_added;
	int4		lcnt, status;
	mname_entry	gvname;
	sgmnt_addrs	*csa;
	sgmnt_data	*csd;
	sm_uc_ptr_t	blkBase, blkEnd, lrecBase, recBase;
	srch_blk_status	dirHist;
	trans_num	lcl_tn;
	unsigned char	key_buff[MAX_KEY_SZ + 3];
	uint4		lcl_bsiz;

	csa = cs_addrs;
	csd = cs_data;
	is_bg = (dba_bg == csd->acc_meth);
	is_mm = (dba_mm == csd->acc_meth);
	dirHist.blk_num = *curr_blk;
	if (NULL == (dirHist.buffaddr = t_qread(dirHist.blk_num, (sm_int_ptr_t)&dirHist.cycle, &dirHist.cr)))
		return (enum cdb_sc)rdfail_detail; /* WARNING assign above - Failed to read the indicated block */
	if (is_bg)
	{	/* for bg try to hold onto the blk */
		if ((CR_NOTVALID == (sm_long_t)(*cr = db_csh_get(dirHist.blk_num))) || (NULL == *cr)) /* WARNING assignment */
			return (enum cdb_sc)rdfail_detail;				/* failed to find the indicated block */
		(*cr)->refer = TRUE;
		curr_blk_cr = **cr;	/* Stash CR for later comparison */
	}
	blkBase = dirHist.buffaddr;
	if (!IS_64_BLK_ID(blkBase))
		return cdb_sc_blkmod;	/* Directory tree blocks should all be V7m */
	lcl_bsiz = ((blk_hdr_ptr_t)blkBase)->bsiz;
	blkEnd = blkBase + lcl_bsiz;
	blk_ver = ((blk_hdr_ptr_t)blkBase)->bver;
	assert(GDSV7m == blk_ver);
	lrecBase = recBase = blkBase + SIZEOF(blk_hdr);
	dirHist.level = level = ((blk_hdr_ptr_t)blkBase)->levl;
	dirHist.tn = lcl_tn = ((blk_hdr_ptr_t)blkBase)->tn;
	gvname.var_name.addr = (char *)key_buff;
	if (0 == level)
	{	/* data (level 0) directory tree blocks point to gvt root blocks */
		DBG_VERIFY_ACCESS(blkEnd - 1);
		if (debug_mupip)
			util_out_print("Region !AD: DT level 0 block 0x!@XQ\tprocessing", TRUE, REG_LEN_STR(reg), curr_blk);
		for (lcl_gv_trees = 0; recBase < blkEnd; recBase +=rec_sz, lcl_gv_trees++)
		{	/* iterate through block invoking upgrade_idx block for each gv root */
			status = read_record(&rec_sz, &key_cmpc, &key_len, key_buff, dirHist.level, &dirHist, recBase);
			if (cdb_sc_normal != status)		/* no *-keys at level 0 in dir tree */
				return status;
			if (3 > key_len)			/* Bad key len, can't subtract KEY_DELIMITER off below */
				return cdb_sc_blkmod;
			gvname.var_name.len = key_len - 2;	/* Ignore trailing double KEY_DELIMITER */
			/* Zero len means a *-key which is disallowed at level 0. Also key_len < (MIDENT + 2 key delim bytes) */
			if ((!key_len) || ((MAX_MIDENT_LEN + 2) < key_len))
				return cdb_sc_blkmod;
			GET_BLK_ID_64(blk_pter, (recBase + SIZEOF(rec_hdr) + key_len));		/* get gvt root block pointer */
			if ((1 >= blk_pter) || (blk_pter > cs_data->trans_hist.total_blks)) /* block_id out of range, retry */
				return cdb_sc_blknumerr;
			if (debug_mupip)
				util_out_print("Region !AD: Global: !AD block 0x!@XQ\tstarted",
						TRUE, REG_LEN_STR(reg), gvname.var_name.len, gvname.var_name.addr, &blk_pter);
			blocks_left = csd->blks_to_upgrd;	/* Remaining blocks to upgrade counter prevents a infinite loop */
			lcnt = MAX_BLK_TRIES;
			do
			{	/* Retry GVT root block until it the upgrade completes cleanly (or control-C) */
				status = upgrade_idx_block(&blk_pter, reg, &gvname, &child_cr);
				if (is_bg && (NULL != child_cr))
				{	/* Release the cache record, transitively making the corresponding buffer, that this
					 * function just modified, avaiable for re-use. Doing so ensures that all the CRs being
					 * touched as part of the REORG UPGRADE do not accumulate creating a situation where "when
					 * everything is special, nothing is special" resulting in parent blocks being moved around
					 * in memory which causes restarts.
					 */
					child_cr->refer = FALSE;
					delete_hashtab_int8(&cw_stagnate, (ublock_id *)&blk_pter);
				}
				if (debug_mupip)
					util_out_print("Region !AD: Global: !AD block 0x!@XQ\tstatus:!UL\tRemaining:!UL",
							TRUE, REG_LEN_STR(reg), gvname.var_name.len, gvname.var_name.addr,
							&blk_pter, status, csd->blks_to_upgrd);
				if ((cdb_sc_normal == status) || mu_ctrlc_occurred)
					break;
				if (blocks_left < csd->blks_to_upgrd)
				{
					blocks_left = csd->blks_to_upgrd;
					lcnt = MAX_BLK_TRIES;
				} else if (status == cdb_sc_blksplit)
				{	/* Block splits do not count as errors, but they do require reprocessing the GVT */
					lcnt = MAX_BLK_TRIES;
					continue;
				} else if ((0 == --lcnt) || (cdb_sc_badlvl == status))
					return status;	/* Repeated/unrecoverable errors, let the caller deal with it */
				/* Use t_retry() here to increment t_tries, grabbing crit as needed. Note that any
				 * downstream t_end() or t_abort() resets t_tries. The result should be that REORG
				 * -UPGRADE will grab and release crit as needed on a per global basis. In the event
				 * that the process does not release crit and still hit a problem, do not retry past
				 * the fourth
				 */
				if (CDB_STAGNATE > t_tries)
					t_retry(status);
				else
					assert((CDB_STAGNATE <= t_tries) && (csa->now_crit));
			} while (TRUE);
			if (mu_ctrlc_occurred)
				break;
			/* Re-read the block in case of a remap/extension */
			if (NULL == (dirHist.buffaddr = t_qread(dirHist.blk_num, (sm_int_ptr_t)&dirHist.cycle, &dirHist.cr)))
				return (enum cdb_sc)rdfail_detail;		/* WARNING assign above - Failed to read block */
			if (is_bg)
			{	/* for bg try to hold onto the blk */
				if ((CR_NOTVALID == (sm_long_t)(*cr = db_csh_get(dirHist.blk_num))) || (NULL == *cr))
					return (enum cdb_sc)rdfail_detail;	/* WARNING assignment above - failed to read block*/
				if ((lcl_tn != ((blk_hdr_ptr_t)dirHist.buffaddr)->tn)
						|| (level != ((blk_hdr_ptr_t)dirHist.buffaddr)->levl)
						|| (lcl_bsiz != ((blk_hdr_ptr_t)dirHist.buffaddr)->bsiz))
					return cdb_sc_losthist;	/* Block TN changed */
				if (dirHist.buffaddr != blkBase)
				{	/* Directory tree buffer changed but the block was not modified. Take corrective action.
					 * These blocks were already upgraded and are less likely to change (TN didn't change)
					 * Reposition recBase and blkEnd before updating blkBase.
					 */
					recBase = dirHist.buffaddr + (recBase - blkBase);
					blkEnd = dirHist.buffaddr + (blkEnd - blkBase);
					blkBase = dirHist.buffaddr;
					assert(blkEnd >= recBase);
				}
				(*cr)->refer = TRUE;
				assert(blkBase == dirHist.buffaddr);
			}
			if (blkBase != dirHist.buffaddr)
				/* Block moved in memory. There are two cases in BG and one in MM
				 * - (BG) Block moved but unchanged which does not require reprocessing
				 * - (BG) Block moved and changed (possible split) which requires reprocessing
				 * - (MM) Database file extension
				 * Note that the BG cases should have been caught by the CR check
				 */
				return cdb_sc_helpedout;	/* Block changed, reprocess recursively */
			if (blkEnd != (blkBase + ((blk_hdr_ptr_t)blkBase)->bsiz))
				return cdb_sc_blkmod;
			assert(dirHist.level == level);
			/* It is possible to have grabbed crit for this GVT even if its upgrade completed. Reset here */
			t_abort(gv_cur_region, csa);
		}
		if ((blkEnd != recBase) & (blkEnd != (blkBase + ((blk_hdr_ptr_t)blkBase)->bsiz)))
			return cdb_sc_blkmod;	/* Problem with reading at the known block end, retry */
		if (debug_mupip)
			util_out_print("Region !AD: DT level 0 block 0x!@XQ\tprocessed with !UL names", TRUE,
					REG_LEN_STR(reg), curr_blk, lcl_gv_trees);
		gv_trees = lcl_gv_trees;
		return cdb_sc_normal;					/* done with this level 0 dt block; return up a level */
	} else
		status = cdb_sc_normal;
	assert(level);
	if (debug_mupip)
		util_out_print("Region !AD: DT level !UL block 0x!@XQ\tprocessing GVTs", TRUE, REG_LEN_STR(reg), level, curr_blk);
	lcl_gv_trees = 0;
	lcnt = MAX_BLK_TRIES;
	while (!mu_ctrlc_occurred && (recBase < blkEnd))
	{	/* process the records in a directory tree index block */
		status = read_record(&rec_sz, &key_cmpc, &key_len, key_buff, level, &dirHist, recBase);
		if ((cdb_sc_normal != status) && (cdb_sc_starrecord != status))
			break;						/* Failed to parse the record */
		GET_BLK_ID_64(blk_pter, (recBase + SIZEOF(rec_hdr) + key_len));
		if ((1 >= blk_pter) || (blk_pter > cs_data->trans_hist.total_blks))
			return cdb_sc_blknumerr;			/* block_id out of range */
		status = find_gvt_roots(&blk_pter, reg, &child_cr);
		if (is_bg && (NULL != child_cr))
		{	/* Release the cache record, transitively making the corresponding buffer, that this function just
			 * modified, avaiable for re-use. Doing so ensures that all the CRs being touched as part of the
			 * REORG UPGRADE do not accumulate creating a situation where "when everything is special, nothing
			 * is special" resulting in parent blocks being moved around in memory which causes restarts.
			 */
			child_cr->refer = FALSE;
		}
		/* Re-read the block immediately to ensure continuity */
		if (NULL == (dirHist.buffaddr = t_qread(dirHist.blk_num, (sm_int_ptr_t)&dirHist.cycle, &dirHist.cr)))
			return rdfail_detail;	/* WARNING assign above - Failed to read block */
		if (is_bg)
		{	/* for bg try to hold onto the blk */
			if ((CR_NOTVALID == (sm_long_t)(*cr = db_csh_get(dirHist.blk_num))) || (NULL == *cr))
				return (enum cdb_sc)rdfail_detail;	/* WARNING assignment above - failed to read block*/
			if ((lcl_tn != ((blk_hdr_ptr_t)dirHist.buffaddr)->tn)
					|| (level != ((blk_hdr_ptr_t)dirHist.buffaddr)->levl)
					|| (lcl_bsiz != ((blk_hdr_ptr_t)dirHist.buffaddr)->bsiz))
				return cdb_sc_losthist;	/* Block TN changed */
			if (dirHist.buffaddr != blkBase)
			{	/* Directory tree buffer changed but the block was not modified. Take corrective action.
				 * These blocks were already upgraded and are less likely to change (TN didn't change)
				 * Reposition recBase and blkEnd before updating blkBase.
				 */
				recBase = dirHist.buffaddr + (recBase - blkBase);
				blkEnd = dirHist.buffaddr + (blkEnd - blkBase);
				blkBase = dirHist.buffaddr;
				assert(blkEnd >= recBase);
			}
			(*cr)->refer = TRUE;
			assert(blkBase == dirHist.buffaddr);
		}
		if (blkBase != dirHist.buffaddr)
			/* Block moved in memory. There are two cases in BG and one in MM
			 * - (BG) Block moved but unchanged which does not require reprocessing
			 * - (BG) Block moved and changed (possible split) which requires reprocessing
			 * - (MM) Database file extension
			 * Note that the BG cases should have been caught by the CR check
			 */
			return cdb_sc_helpedout;			/* Block changed, reprocess recursively */
		if (blkEnd != (blkBase + ((blk_hdr_ptr_t)blkBase)->bsiz))
			return cdb_sc_blkmod;				/* Block changed, reprocess recursively */
		if (GDSV7m != ((blk_hdr_ptr_t)blkBase)->bver)
			return cdb_sc_blkmod;				/* Block changed, reprocess recursively */
		assert(dirHist.level == level);
		level = ((blk_hdr_ptr_t)blkBase)->levl;				/* might have increased due to split propagation */
		if ((cdb_sc_normal != status) && (cdb_sc_starrecord != status))
		{
			if ((lrecBase == recBase) && (0 >= lcnt--))	/* Too many repeats on the same record */
				return status;				/* Block re-read is necessary */
			continue;					/* Otherwise, retry the record */
		}
		lrecBase = recBase;
		lcnt = MAX_BLK_TRIES;
		recBase += ((rec_hdr_ptr_t)recBase)->rsiz;
		lcl_gv_trees++;
		assert(blkEnd >= recBase);
	}
	if ((blkEnd != recBase) || (blkEnd != (blkBase + ((blk_hdr_ptr_t)blkBase)->bsiz)))
		return cdb_sc_blkmod;	/* Record did not end at the block size, retry */
	if (debug_mupip)
		util_out_print("Region !AD: DT level !UL block 0x!@XQ\tprocessed !UL GVTs", TRUE,
				REG_LEN_STR(reg), level, curr_blk, lcl_gv_trees);
	return status;
}

/******************************************************************************************
 * This recursively traverses a global variable tree and and upgrades the pointers in index
 * blocks, which likely entails block splitting. It works down levels, because splits that
 * add levels then create new blocks in the desired format to match the upgraded block
 * being split, works index blocks from left to right, grabbing crit for each upgrade
 * As mentioned above, it upgrades all the pointers from 32-bit to 64-bit format; which
 * is unlikely to be needed in the event of any subsequent master map extentions, nor
 * address the needs of any future block format changes.
 *
 * Input Parameters:
 * 	curr_blk identifies an index block
 * 	reg points to the base structure for the region
 * 	gvname points to an mname_entry containing key text
 * Output Parameters:
 * 	*cr points to the cache record that this function was working on for the caller to release
 * 	(enum_cdb_sc) returns cdb_sc_normal which the code expects or a retry code
 ******************************************************************************************/
enum cdb_sc upgrade_idx_block(block_id *curr_blk, gd_region *reg, mname_entry *gvname, cache_rec_ptr_t *cr)
{
	blk_hdr		blkHdr;
	blk_segment	*bs1, *bs_ptr;
	block_id	blk_pter;
	boolean_t	is_bg, is_mm;
	cache_rec	dummy_gvt_cr, curr_blk_cr;
	cache_rec_ptr_t	child_cr;
	enum db_ver	blk_ver;
	gvnh_reg_t	*gvnh_reg = NULL;
	int		blk_seg_cnt, i, key_cmpc, key_len, level, max_fill, max_key, max_rightblk_lvl, new_blk_sz, num_recs, rec_sz,
			space_need, split_blks_added, split_levels_added, tmp_len, v7_rec_sz;
	int4		blk_size, child_data_blks, status;
	mname_entry	gvt_name;
	sgmnt_addrs	*csa;
	sm_uc_ptr_t	blkBase, blkEnd, recBase, v7bp, v7end, v7recBase;
	srch_blk_status	blkHist, *curr_blk_hist_ptr;
	srch_hist	alt_hist;
	trans_num	lcl_tn;
	unsigned char	gname[SIZEOF(mident_fixed) + 2], key_buff[MAX_KEY_SZ + 3];
	uint4		lcl_bsiz;

	csa = cs_addrs;
	is_mm = (dba_mm == cs_data->acc_meth);
	is_bg = (dba_bg == cs_data->acc_meth);
	memcpy(gv_currkey->base, gvname->var_name.addr, gvname->var_name.len + 1);
	gv_currkey->end = gvname->var_name.len + 1;
	gv_currkey->base[gv_currkey->end++] = KEY_DELIMITER;
	gv_currkey->base[gv_currkey->end] = KEY_DELIMITER;
	assert(!csa->now_crit || (CDB_STAGNATE <= t_tries));
	assert(!update_trans && !need_kip_incr);
	if (debug_mupip)
		util_out_print("In index block 0x!@XQ keyed with !AD", TRUE, curr_blk, gvname->var_name.len, gvname->var_name.addr);
	gvt_name.var_name.addr = (char *)key_buff;
	gvt_name.var_name.len = 0;
	blkHist.blk_num = *curr_blk;
	if (NULL == (blkHist.buffaddr = t_qread(blkHist.blk_num, (sm_int_ptr_t)&blkHist.cycle, &blkHist.cr)))	/* WARNING assign */
		return (enum cdb_sc)rdfail_detail; /* WARNING assign above - Failed to read the indicated block */
	if (is_bg)
	{	/* for bg try to hold onto the blk */
		if ((CR_NOTVALID == (sm_long_t)(*cr = db_csh_get(blkHist.blk_num))) || (NULL == *cr)) /* WARNING assignment */
			return (enum cdb_sc)rdfail_detail;	/* failed to find the indicated block */
		(*cr)->refer = TRUE;
		curr_blk_cr = **cr;	/* Stash CR for later comparison because the read is done before t_begin() */
	}
	blkBase = blkHist.buffaddr;
	blkHdr = *((blk_hdr_ptr_t)blkBase);
	new_blk_sz = lcl_bsiz = blkHdr.bsiz;
	blkEnd = blkBase + new_blk_sz;
	blkHist.level = level = blkHdr.levl;
	blkHist.tn = lcl_tn = blkHdr.tn;
	blk_ver = blkHdr.bver;
	if (0 == level)	/* Caller intended to change an index block. The parent needs to be reprocessed */
		return cdb_sc_badlvl;
	if (debug_mupip)
		util_out_print("!UL:0x!@XQ keyed with !AD from !@XQ", TRUE,
				level, curr_blk, gvname->var_name.len, gvname->var_name.addr, &lcl_tn);
	status = cdb_sc_normal;
	if (GDSV7m > blk_ver)
	{	/* block needs pointers upgraded */
		/* check how much space is need to upgrade the block and whether the block needs splitting */
		if (debug_mupip)
			util_out_print("!UL:0x!@XQ Pointer upgrade needed", TRUE, level, curr_blk);
		for (num_recs = 0, recBase = blkBase + sizeof(blk_hdr); recBase < blkEnd; recBase += rec_sz, num_recs++)
		{	/* process index block to get space required; management of crit is delicate: unlike DT defer recursion */
			if (is_bg)
			{
				if ((CR_NOTVALID == (sm_long_t)(*cr = db_csh_get(blkHist.blk_num))) || (NULL == *cr))
					return (enum cdb_sc)rdfail_detail;	/* WARNING assignment above - failed to read block*/
				if ((lcl_tn != ((blk_hdr_ptr_t)GDS_REL2ABS((*cr)->buffaddr))->tn)	/* Use CR w/o t_qread() */
						|| (level != ((blk_hdr_ptr_t)GDS_REL2ABS((*cr)->buffaddr))->levl)
						|| (lcl_bsiz != ((blk_hdr_ptr_t)GDS_REL2ABS((*cr)->buffaddr))->bsiz))
					return cdb_sc_losthist;	/* Block TN changed */
				if (((sm_uc_ptr_t)GDS_REL2ABS((*cr)->buffaddr) != blkBase) || (curr_blk_cr.cycle != (*cr)->cycle))
					return cdb_sc_lostcr;	/* Lost the CR - let the caller sort it out */
			}
			if (blkHist.buffaddr != blkBase)
			{
				assert(is_mm);
				return cdb_sc_lostcr;	/* Block moved - let the caller sort it out */
			}
			status = read_record(&rec_sz, &key_cmpc, &key_len, key_buff, level, &blkHist, recBase);
			if (cdb_sc_starrecord == status)
			{	/* *-key has no explicit key */
				if ((((rec_hdr_ptr_t)recBase)->rsiz + recBase != blkEnd) || (0 == level) || (0 != key_len))
					return cdb_sc_blkmod; /* Star record doesn't end correctly, restart */
			} else if (cdb_sc_normal != status) /* failed to parse record */
				return status;
			new_blk_sz += (SIZEOF_BLK_ID(BLKID_64) - SIZEOF_BLK_ID(BLKID_32));
		}
		if ((blkEnd != recBase) || (blkEnd != (blkBase + ((blk_hdr_ptr_t)blkBase)->bsiz)))
			return cdb_sc_blkmod;	/* Record did not end at the block size, retry */
		recBase = blkBase + SIZEOF(blk_hdr);
		blk_size = cs_data->blk_size;
		if (csa->now_crit)
		{	/* Have to let go of crit ahead of the t_begin() and gen_hist_for_blk() below */
			rel_crit(gv_cur_region);
			assert(CDB_STAGNATE <= t_tries);
			t_tries = 0;
		}
		status = gen_hist_for_blk(&blkHist, blkBase, recBase, gvname, gvnh_reg);
		if (cdb_sc_normal != status)	/* Failed to generate a history for the block */
		{
			if (cdb_sc_starrecord != status)
				return status;
			/* Got a star record, use bespoke history for t_end */
			curr_blk_hist_ptr = &alt_hist.h[0];
		} else	/* Use the real history pointer for t_end */
		{
			alt_hist = gv_target->hist;
			curr_blk_hist_ptr = &alt_hist.h[level];
		}
		assert(level);
		assert(new_blk_sz == (lcl_bsiz + num_recs * (SIZEOF_BLK_ID(BLKID_64) - SIZEOF_BLK_ID(BLKID_32))));
		t_begin(ERR_MUNOUPGRD, UPDTRNS_DB_UPDATED_MASK);
		while (0 < (space_need = new_blk_sz - blk_size))				/* WARNING assignment */
		{	/* Insufficient room; WARNING: using a loop construct to enable an alternate pathway out of this level */
			if (2 == num_recs)
			{	/* This block contains only two records:
				*  - one regular (but really large) record with a block pointer
				*  - a star-record
				* If we need space, that implies that there is insufficient space to upgrade both block
				* pointers from 4 bytes to 8 bytes. If the user tried to set this global with the current
				* version, it would thrown a GVSUBOFLOW error. So that is what happens here. This error stops
				* the upgrade because it cannot handle this situation.
				*/
				t_abort(gv_cur_region, csa);					/* do crit and other cleanup */
				tmp_len = gv_currkey->end;
				max_key = (NULL != gv_cur_region) ? gv_cur_region->max_key_size : 0 ;
				ISSUE_GVSUBOFLOW_ERROR(gv_currkey, KEY_COMPLETE_TRUE, tmp_len, max_key, gv_cur_region);
			}
			max_fill = (new_blk_sz >> 1);
			if (debug_mupip)
				util_out_print("!UL:0x!@XQ\tSPLIT have:!UL need:!UL", TRUE, level, curr_blk, new_blk_sz, max_fill);
			gv_target->clue.end = 0;	/* Invalidate the clue */
			split_blks_added = split_levels_added = 0;
			mu_reorg_process = TRUE;
			status = mu_split(level, max_fill, max_fill, &split_blks_added, &split_levels_added, &max_rightblk_lvl);
			mu_reorg_process = FALSE;
			if (cdb_sc_normal != status)
			{	/* split failed */
				t_abort(gv_cur_region, csa);					/* do crit and other cleanup */
				return status;
			}
			mu_upgrade_in_prog = MUPIP_UPGRADE_OFF;	/* Splits can affect blocks_to_upgrd, temporarily disable */
			inctn_opcode = inctn_blkupgrd;
			inctn_detail.blknum_struct.blknum = *curr_blk;
			mu_reorg_upgrd_dwngrd_blktn = lcl_tn;
			if (IS_ONLNRLBK_ACTIVE(csa))
			{
				t_abort(gv_cur_region, csa);					/* do crit and other cleanup */
				return cdb_sc_onln_rlbk1;
			}
			if ((trans_num)0 == t_end(&(gv_target->hist), NULL, TN_NOT_SPECIFIED))
			{	/* failed to commit the split */
				mu_upgrade_in_prog = MUPIP_REORG_UPGRADE_IN_PROGRESS;
				t_abort(gv_cur_region, csa);					/* do crit and other cleanup */
				return (enum cdb_sc)t_fail_hist[t_tries];
			}
			mu_upgrade_in_prog = MUPIP_REORG_UPGRADE_IN_PROGRESS;
			assert((((blk_hdr_ptr_t)blkBase)->bsiz <= blk_size) && split_blks_added);
			tot_splt_cnt += split_blks_added;
			tot_levl_cnt += split_levels_added;
			if (debug_mupip)
				util_out_print("!UL:0x!@XQ\tSPLIT - done (added: !UL, levels: !UL)", TRUE, level,
							curr_blk, split_blks_added, split_levels_added);
			return cdb_sc_blksplit;
		}
		/* Finally actually upgrade the block */
		if (NULL == (blkHist.buffaddr = t_qread(blkHist.blk_num, (sm_int_ptr_t)&blkHist.cycle, &blkHist.cr)))
		{	/* WARNING assign above */
			t_abort(gv_cur_region, csa);		/* do crit and other cleanup */
			return (enum cdb_sc)rdfail_detail;	/* WARNING assignment above - failed to read block*/
		}
		if (is_bg)
		{
			if ((CR_NOTVALID == (sm_long_t)(*cr = db_csh_get(blkHist.blk_num))) || (NULL == *cr))
			{
				t_abort(gv_cur_region, csa);		/* do crit and other cleanup */
				return (enum cdb_sc)rdfail_detail;	/* WARNING assignment above - failed to read block*/
			}
			if ((lcl_tn != ((blk_hdr_ptr_t)blkHist.buffaddr)->tn)
					|| (level != ((blk_hdr_ptr_t)blkHist.buffaddr)->levl)
					|| (lcl_bsiz != ((blk_hdr_ptr_t)blkHist.buffaddr)->bsiz))
			{	/* Block TN changed */
				t_abort(gv_cur_region, csa);		/* do crit and other cleanup */
				return cdb_sc_losthist;
			}
			if ((blkHist.buffaddr != blkBase) || (curr_blk_cr.cycle != (*cr)->cycle))
			{	/* Lost the CR - let the caller sort it out */
				t_abort(gv_cur_region, csa);		/* do crit and other cleanup */
				return cdb_sc_lostcr;
			}
		}
		if (blkHist.buffaddr != blkBase)
		{	/* Block moved - let the caller sort it out */
			t_abort(gv_cur_region, csa);			/* do crit and other cleanup */
			return cdb_sc_lostcr;
		}
		assert(GDSV7m != blk_ver);
		assert(blkHist.buffaddr == blkBase);
		CHECK_AND_RESET_UPDATE_ARRAY;
		BLK_INIT(bs_ptr, bs1);
		BLK_ADDR(v7bp, new_blk_sz, unsigned char);
		v7recBase = v7bp + SIZEOF(blk_hdr);
		((blk_hdr_ptr_t)v7bp)->bsiz = new_blk_sz;
		v7end = v7bp + new_blk_sz;
		recBase = blkBase + SIZEOF(blk_hdr);
		for (child_data_blks = 1; recBase < blkEnd; child_data_blks++, recBase += rec_sz, v7recBase += v7_rec_sz)
		{	/* Update the recBase and v7recBase pointers to point to the next record */
			/* Because blocks have pointers rather than application data, no spanning & bsiz not a worry */
			status = read_record(&rec_sz, &key_cmpc, &key_len, key_buff, level, &blkHist, recBase);
			if ((cdb_sc_normal != status) && (cdb_sc_starrecord != status))
			{	/* failed to parse the record */
				t_abort(gv_cur_region, csa);					/* do crit and other cleanup */
				return status;
			}
			if ((recBase + rec_sz) > blkEnd)
			{
				t_abort(gv_cur_region, csa);					/* do crit and other cleanup */
				return cdb_sc_badoffset;
			}
			GET_BLK_ID_32(blk_pter, (recBase + SIZEOF(rec_hdr) + key_len));
			if ((csa->ti->total_blks < blk_pter) && (0 > blk_pter))
			{	/* Block pointer is bad, should not get here unless the block was concurrently modified */
				t_abort(gv_cur_region, csa);					/* do crit and other cleanup */
				return cdb_sc_blknumerr;					/* block_id out of range */
			}
			v7_rec_sz = rec_sz + (SIZEOF_BLK_ID(BLKID_64) - SIZEOF_BLK_ID(BLKID_32));
			assert(blk_size > v7_rec_sz);
			if ((v7end < (v7recBase + v7_rec_sz)) || (num_recs < (child_data_blks - 1)))
			{	/* About to copy more than expected and/or the record count changed due to concurrent activity */
				t_abort(gv_cur_region, csa);					/* do crit and other cleanup */
				return cdb_sc_badoffset;
			}
			/* Copy the revised record into the update array */
			memcpy(v7recBase, recBase, SIZEOF(rec_hdr) + key_len);
			assert((unsigned short)v7_rec_sz == v7_rec_sz);
			((rec_hdr_ptr_t)v7recBase)->rsiz = (unsigned short)v7_rec_sz;
			PUT_BLK_ID_64((v7recBase + SIZEOF(rec_hdr) + key_len), blk_pter);
			if (debug_mupip)
				util_out_print("!UL:0x!@XQ\tUpgraded pointer:!@XQ", TRUE, level, curr_blk, &blk_pter);
		}
		if (is_bg)
		{
			if ((CR_NOTVALID == (sm_long_t)(*cr = db_csh_get(blkHist.blk_num))) || (NULL == *cr))
			{	/* WARNING assignment above - failed to read block*/
				t_abort(gv_cur_region, csa);					/* do crit and other cleanup */
				return (enum cdb_sc)rdfail_detail;
			}
			if ((lcl_tn != ((blk_hdr_ptr_t)GDS_REL2ABS((*cr)->buffaddr))->tn)	/* Use CR w/o t_qread() */
					|| (level != ((blk_hdr_ptr_t)GDS_REL2ABS((*cr)->buffaddr))->levl)
					|| (lcl_bsiz != ((blk_hdr_ptr_t)GDS_REL2ABS((*cr)->buffaddr))->bsiz))
			{	/* Block TN changed */
				t_abort(gv_cur_region, csa);					/* do crit and other cleanup */
				return cdb_sc_losthist;
			}
			if (((sm_uc_ptr_t)GDS_REL2ABS((*cr)->buffaddr) != blkBase) || (curr_blk_cr.cycle != (*cr)->cycle))
			{	/* Lost the CR - let the caller sort it out */
				t_abort(gv_cur_region, csa);					/* do crit and other cleanup */
				return cdb_sc_lostcr;
			}
		}
		if (blkHist.buffaddr != blkBase)
		{	/* Block moved - let the caller sort it out */
			t_abort(gv_cur_region, csa);						/* do crit and other cleanup */
			assert(is_mm);			/* bg checks should have stopped this */
			return cdb_sc_lostcr;
		}
		if ((blkEnd != recBase) & (blkEnd != (blkBase + ((blk_hdr_ptr_t)blkBase)->bsiz)))
		{	/* Problem with reading at the known block end, retry */
			t_abort(gv_cur_region, csa);						/* do crit and other cleanup */
			assert(is_mm);			/* bg checks should have stopped this */
			return cdb_sc_blkmod;
		}
		BLK_SEG(bs_ptr, v7bp + SIZEOF(blk_hdr), new_blk_sz - SIZEOF(blk_hdr));
		assert(blk_seg_cnt == new_blk_sz);
		if (!BLK_FINI(bs_ptr, bs1))
		{	/* failed to finalize the update */
			t_abort(gv_cur_region, csa);						/* do crit and other cleanup */
			return cdb_sc_blkmod;
		}
		if (debug_mupip)
			util_out_print("!UL:0x!@XQ\tFinalizing block !@XQ", TRUE, level, curr_blk, &start_tn);
		if (&alt_hist.h[0] == curr_blk_hist_ptr)
		{	/* Bespoke history for star record for t_end */
			assert(*cr == blkHist.cr);
			dummy_gvt_cr.ondsk_blkver = GDSV7m;	/* t_write uses this to apply the block version */
			blkHist.cr = &dummy_gvt_cr;		/* MM: has no CR; BG: don't modify the in-memory CR */
			alt_hist.depth = 1;
			alt_hist.h[0] = blkHist;		/* Use the latest information from t_qread() */
			alt_hist.h[0].tn = start_tn;
			alt_hist.h[0].blk_target = NULL;	/* So that t_end doesn't expect to find a gvnh */
			alt_hist.h[1].blk_num = 0;		/* Stop history reading */
		}
		curr_blk_hist_ptr->cse = t_write(curr_blk_hist_ptr, bs1, 0, 0, level, TRUE, TRUE, GDS_WRITE_KILLTN);
		curr_blk_hist_ptr->cse->ondsk_blkver = GDSV7m;	/* Update the block version */
		if (is_bg && (&alt_hist.h[0] == curr_blk_hist_ptr)) /* Restore the CR pointer */
			alt_hist.h[0].cr = curr_blk_hist_ptr->cse->cr = *cr;
		inctn_opcode = inctn_blkupgrd;
		inctn_detail.blknum_struct.blknum = *curr_blk;
		mu_reorg_upgrd_dwngrd_blktn = ((blk_hdr_ptr_t)blkBase)->tn;
		if (IS_ONLNRLBK_ACTIVE(csa))
		{
			t_abort(gv_cur_region, csa);					/* do crit and other cleanup */
			return cdb_sc_onln_rlbk1;
		}
		if (0 == (lcl_tn = t_end(&alt_hist, NULL, TN_NOT_SPECIFIED)))
		{	/* failed to commit the block revision */
			t_abort(gv_cur_region, csa);						/* do crit and other cleanup */
			return (enum cdb_sc)t_fail_hist[t_tries];
		}
		lcl_bsiz = new_blk_sz;
		if (1 == level)
			tot_data_blks += child_data_blks;
		else
			tot_data_blks++;
		if (debug_mupip)
			util_out_print("!UL:0x!@XQ\tUpgraded block at !@XQ", TRUE, level, curr_blk, &start_tn);
		tot_dt++;
	} else
	{
		if (debug_mupip)
			util_out_print("!UL:0x!@XQ\tRepeat", TRUE, level, curr_blk);
		index_blks_at_v7++;
	}
	if (1 < level)
	{	/* go after descendent trees below */
		blkEnd = blkBase + ((blk_hdr_ptr_t)blkBase)->bsiz;
		for (recBase = blkBase + SIZEOF(blk_hdr); recBase < blkEnd;)
		{
			if (NULL == (blkHist.buffaddr = t_qread(blkHist.blk_num, (sm_int_ptr_t)&blkHist.cycle, &blkHist.cr)))
				return (enum cdb_sc)rdfail_detail; /* WARNING assign above - Failed to read the indicated block */
			if (is_bg)
			{	/* for bg try to hold onto the blk */
				if ((CR_NOTVALID == (sm_long_t)(*cr = db_csh_get(blkHist.blk_num)))	/* WARNING assign */
						|| (NULL == *cr))
					return (enum cdb_sc)rdfail_detail;		/* failed to find the indicated block */
				if (blkBase == recBase)	/* 1st read of t_end, stash CR for later comparison */
					curr_blk_cr = **cr;
				if ((lcl_tn != ((blk_hdr_ptr_t)blkHist.buffaddr)->tn)
						|| (level != ((blk_hdr_ptr_t)blkHist.buffaddr)->levl)
						|| (lcl_bsiz != ((blk_hdr_ptr_t)blkHist.buffaddr)->bsiz))
					return cdb_sc_losthist;	/* Block TN changed */
				if (blkHist.buffaddr != blkBase)
				{	/* Index block buffer changed but the block was not modified. Take corrective action.
					 * This block was already upgraded and should be less likely to change (TN didn't change)
					 * Reposition recBase and blkEnd before updating blkBase.
					 */
					recBase = blkHist.buffaddr + (recBase - blkBase);
					blkEnd = blkHist.buffaddr + (blkEnd - blkBase);
					blkBase = blkHist.buffaddr;
					assert(blkEnd >= recBase);
				}
				(*cr)->refer = TRUE;
				assert(blkBase == blkHist.buffaddr);
			}
			if (blkBase != blkHist.buffaddr)
				/* Block moved in memory. There are two cases in BG and one in MM
				 * - (BG) Block moved but unchanged which does not require reprocessing
				 * - (BG) Block moved and changed (possible split) which requires reprocessing
				 * - (MM) Database file extension
				 * Note that the BG cases should have been caught by the CR check
				 */
				return cdb_sc_helpedout;	/* Block changed, reprocess recursively */
			if (blkEnd != (blkBase + ((blk_hdr_ptr_t)blkBase)->bsiz))
				return cdb_sc_blkmod;
			status = read_record(&rec_sz, &key_cmpc, &key_len, key_buff, level, &blkHist, recBase);
			if (cdb_sc_starrecord == status)
			{	/* Initialize name for *-key */
				if ((((rec_hdr_ptr_t)recBase)->rsiz + recBase != blkEnd) || (0 == level) || (0 != key_len))
					return cdb_sc_blkmod; /* Star record doesn't end correctly, restart */
				gvt_name.var_name.len = gvname->var_name.len;
				memcpy(gvt_name.var_name.addr, gvname->var_name.addr, gvt_name.var_name.len);
			} else if (cdb_sc_normal != status)
				return status;	/* failed to parse record */
			else if (3 > key_len)	/* Bad key len, can't subtract KEY_DELIMITER off below */
				return cdb_sc_blkmod;
			else	/* gvt_name contains the key plus the (unwanted) trailing 2x KEY_DELIMITER */
				gvt_name.var_name.len = key_len - 2; /* subtract trailing 2x KEY_DELIMITER */
			assert(0 < gvt_name.var_name.len);
			GET_BLK_ID_64(blk_pter, (recBase + SIZEOF(rec_hdr) + key_len));
			if ((1 >= blk_pter) || (blk_pter > cs_data->trans_hist.total_blks))
				return cdb_sc_blknumerr;	/* Block out of range, retry */
			if (debug_mupip)
				util_out_print("!UL:0x!@XQ descending to child block 0x!@XQ", TRUE, level, curr_blk, &blk_pter);
			status = upgrade_idx_block(&blk_pter, reg, &gvt_name, &child_cr);
			if (is_bg && (NULL != child_cr))
			{	/* Release the cache record, transitively making the corresponding buffer, that this function just
				 * modified, avaiable for re-use. Doing so ensures that all the CRs being touched as part of the
				 * REORG UPGRADE do not accumulate creating a situation where "when everything is special, nothing
				 * is special" resulting in parent blocks being moved around in memory which causes restarts.
				 */
				child_cr->refer = FALSE;
				delete_hashtab_int8(&cw_stagnate, (ublock_id *)&blk_pter);
			}
			if (cdb_sc_blksplit == status)
				return cdb_sc_blksplit;	/* Block split occurred. Alert the caller to reprocess the entire block */
			else if (cdb_sc_normal != status)
				return status;
			recBase += rec_sz;
		}
		if ((blkEnd != recBase) || (blkEnd != (blkBase + ((blk_hdr_ptr_t)blkBase)->bsiz)))
			return cdb_sc_blkmod;	/* Record did not end at the block size, retry */
	}
	if ((cdb_sc_normal != status) && (cdb_sc_starrecord != status))
		return status;
	if (debug_mupip)
		util_out_print("!UL:0x!@XQ Upgrade complete at 0x!@XQ (including descendants)", TRUE, level, curr_blk, &start_tn);
	return cdb_sc_normal;									/* finished upgrading this block */
}
