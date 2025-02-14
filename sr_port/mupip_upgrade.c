/****************************************************************
 *								*
 * Copyright (c) 2005-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* mupip_upgrade.c: Driver program to upgrade the following.
 * 1) V6.x database files (max of 992Mi blocks) to V7.0 format (max of 16Gi blocks)
 */

#include "mdef.h"

#include "gdsroot.h"
#include "gdsbml.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "v6_gdsfhead.h"
#include "gtm_string.h"
#include "gtm_common_defs.h"
#include "util.h"
#include "filestruct.h"
#include "cli.h"
#include "muextr.h"
#include "memcoherency.h"
#include "interlock.h"
#include "jnl.h"	/* For fd_type */

/* Prototypes */
#include "anticipatory_freeze.h"
#include "change_reg.h"
#include "db_header_conversion.h"
#include "db_ipcs_reset.h"
#include "do_semop.h"
#include "eintr_wrappers.h"
#include "gtmio.h"
#include "gdskill.h"
#include "gtm_semutils.h"
#include "gtm_stat.h"
#include "gtm_stdlib.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gvcst_protos.h"
#include "mu_getlst.h"
#include "mu_reorg.h"
#include "mu_rndwn_file.h"
#include "mu_upgrade_bmm.h"
#include "mucblkini.h"
#include "mupip_exit.h"
#include "mupip_reorg.h"
#include "mupip_upgrade.h"
#include "mu_gv_cur_reg_init.h"
#include "mu_upgrd_dngrd_confirmed.h"
#include "spec_type.h"
#include "targ_alloc.h"
#include "verify_db_format_change_request.h"
#include "wcs_flu.h"
#include "wcs_mm_recover.h"	/* For CHECK_MM_DBFILEXT_REMAP_IF_NEEDED */
#include "wcs_recover.h"

#ifdef __MVS__
#include "gtm_zos_io.h"
#endif
#include "iosp.h"
#include "v15_gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "v15_gdsbt.h"
#include "v15_gdsfhead.h"
#include "v15_filestruct.h"
#include "gds_rundown.h"
#include "error.h"
#include "gtmmsg.h"

/* Because the process has standalone access, it must be released before proceeding to exit */
#define RELEASE_ACCESS_SEMAPHORE_AND_RUNDOWN(REG, ERR)								\
MBSTART {													\
	int4			save_errno;									\
	unix_db_info		*udi;										\
	udi = FILE_INFO(REG);											\
	assert(udi->grabbed_access_sem);									\
	if (0 != (save_errno = do_semop(udi->semid, DB_CONTROL_SEM, -1, SEM_UNDO)))				\
	{													\
		assert(FALSE);	/* we hold it, so we should be able to release it*/				\
		rts_error_csa(CSA_ARG(REG2CSA(REG)) VARLSTCNT(12) ERR_CRITSEMFAIL, 2, DB_LEN_STR(REG),		\
				ERR_SYSCALL, 5, RTS_ERROR_LITERAL("semop()"), CALLFROM, save_errno);		\
	}													\
	udi->grabbed_access_sem = FALSE;									\
	ERR |= gds_rundown(CLEANUP_UDI_TRUE); /* Rundown stops times for prior regions */			\
} MBEND

GBLREF	bool			error_mupip;
GBLREF	boolean_t		debug_mupip;
GBLREF	uint4			mu_upgrade_in_prog;		/* 1 when MUPIP UPGRADE is in progress */
GBLREF	gd_region		*gv_cur_region;
GBLREF	gd_addr			*gd_header;
GBLREF	gv_namehead		*gv_target_list, *reorg_gv_target;
GBLREF	inctn_detail_t		inctn_detail;			/* holds detail to fill in to inctn jnl record */
GBLREF	inctn_opcode_t		inctn_opcode;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	tp_region		*grlist;
GBLREF	uint4			process_id;

#define V6TOV7UPGRADEABLE 1

error_def(ERR_CRITSEMFAIL);
error_def(ERR_DBRNDWN);
error_def(ERR_DBFILOPERR);
error_def(ERR_DBPREMATEOF);
error_def(ERR_DBRDERR);
#ifndef V6TOV7UPGRADEABLE	/* Upgrades currently disbaled */
error_def(ERR_GTMCURUNSUPP);
#endif
error_def(ERR_MUNOACTION);
error_def(ERR_MUNODBNAME);
error_def(ERR_MUNOFINISH);
error_def(ERR_MUNOTALLSEC);
error_def(ERR_MUQUALINCOMP);
error_def(ERR_MUSTANDALONE);
error_def(ERR_TEXT);

void mupip_upgrade(void)
{
	boolean_t		error = FALSE, file, region, was_asyncio_enabled, was_crit;
	block_id		blks_in_way, old_vbn;
	char			buff[OUT_BUFF_SIZE], *errptr, fn[MAX_FN_LEN + 1];
	enum jnl_state_codes	jnl_state;
	enum repl_state_codes	repl_state;
	gd_region		*reg;
	gd_segment		*seg;
	gv_namehead		*gvt;
	inctn_opcode_t		save_inctn_opcode;
	int			exit_status, fd, close_res, rc, save_errno, split_blks_added, split_levels_added;
	int4			blk_size, bmls_to_work, lcl_max_key_size, new_bmm_size, status;
	jnl_buffer_ptr_t	jbp;
	jnl_private_control	*jpc;
	size_t			blocks_needed, fn_len;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd, pvt_csd = NULL;
	sm_uc_ptr_t		bmm_base, bml_buff;
	tp_region		*rptr, single;
	uint4			jnl_status, sts;
	unix_db_info		*udi;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Structure checks */
#ifndef V6TOV7UPGRADEABLE	/* Reject until upgrades are enabled */
	mupip_exit(ERR_GTMCURUNSUPP);
#endif
	assert((8192) == SIZEOF(sgmnt_data));		/* Verify V7 file header hasn't changed */
	assert((8192) == SIZEOF(v6_sgmnt_data));	/* Verify V6 file header hasn't changed */
	error = file = FALSE;
	/* DBG qualifier prints extra debug messages */
	debug_mupip = (CLI_PRESENT == cli_present("DBG"));
	/* Get list of regions to upgrade */
	file = (CLI_PRESENT == cli_present("FILE"));
	region = (CLI_PRESENT == cli_present("REGION")) || (CLI_PRESENT == cli_present("R"));
	TREF(statshare_opted_in) = NO_STATS_OPTIN;	/* Do not open statsdb automatically when basedb is opened */
	mu_upgrade_in_prog = MUPIP_UPGRADE_IN_PROGRESS;	/* needed to signal "zgbldir_opt()" (called inside "gvinit()")
							 * to not process YDBENVINDX_APP_ENSURES_ISOLATION env var.
							 * See comment there for more details.
							 */
	gvinit();				/* initialize gd_header (needed by the later call to mu_getlst) and gv_keysize */
	mu_upgrade_in_prog = MUPIP_UPGRADE_OFF;
	if ((file == region) && (TRUE == file))
	{
		mupip_exit(ERR_MUQUALINCOMP);
		assert(FALSE);
		rptr = NULL;	/* to silence [-Wsometimes-uninitialized] compiler warning */
	} else if (file)
	{
		mu_gv_cur_reg_init();
		seg = gv_cur_region->dyn.addr;
		seg->fname_len = MAX_FN_LEN;
		if (!cli_get_str("WHAT",  (char *)&seg->fname[0], &seg->fname_len))
			mupip_exit(ERR_MUNODBNAME);
		seg->fname[seg->fname_len] = '\0';
		rptr = &single;		/* a dummy value that permits one trip through the loop */
		rptr->fPtr = NULL;
		rptr->reg = gv_cur_region;
		gd_header = gv_cur_region->owning_gd;
		insert_region(gv_cur_region, &(grlist), NULL, SIZEOF(tp_region));
	} else
	{	/* Region required */
		mu_getlst("REGION", SIZEOF(tp_region));
		rptr = grlist;	/* setup of grlist down implicitly by insert_region() called in mu_getlst() */
		if (error_mupip)
		{
			util_out_print("!/MUPIP UPGRADE cannot proceed with above errors!/", TRUE);
			mupip_exit(ERR_MUNOACTION);
		}
	}
	if (!mu_upgrd_dngrd_confirmed())
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TEXT, 2, LEN_AND_LIT("Upgrade canceled by user"));
		mupip_exit(ERR_MUNOACTION);
	}
	/* Iterate over regions */
	for ( ;  NULL != rptr;  rptr = rptr->fPtr)
	{	/* Iterate over regions once to get master map upgrades over with quickly */
		reg = rptr->reg;					/* Initialize the region to check if eligible for upgrade */
		gv_cur_region = reg;
		/* Override name map so that all names map to the current region */
		remap_globals_to_one_region(gd_header, gv_cur_region);
		gvcst_init(reg);
		change_reg();
		csa = cs_addrs;
		if (csa->hdr->read_only)
		{
			error = TRUE;
			util_out_print("Region !AD : Region is read-only, skipping database !AD",
			       TRUE, REG_LEN_STR(reg), DB_LEN_STR(reg));
			if (EXIT_NRM != gds_rundown(CLEANUP_UDI_TRUE))
			{	/* Failed to rundown the DB */
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_DBRNDWN, 2, DB_LEN_STR(gv_cur_region));
				util_out_print(
					"Region !AD : Failed to rundown the database (!AD) in order to get standalone access.",
					TRUE, REG_LEN_STR(reg), DB_LEN_STR(reg));
			}
			continue;
		}
		if (was_asyncio_enabled = csa->hdr->asyncio)	/* WARNING assignment */
		{
			fn_len = (size_t)rptr->reg->dyn.addr->fname_len + 1; /* Includes null terminator */
			assert(sizeof(fn) >= fn_len);
			memcpy(fn, (char *)rptr->reg->dyn.addr->fname, fn_len);
		} else
			fn_len = 0;
		/* Obtain standalone access */
		if (EXIT_NRM != gds_rundown(CLEANUP_UDI_FALSE))
		{	/* Failed to rundown the DB, so we can't get standalone access */
			error = TRUE;
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_DBRNDWN, 2, DB_LEN_STR(gv_cur_region));
			util_out_print("Region !AD : Failed to rundown the database (!AD) in order to get standalone access.",
			       TRUE, REG_LEN_STR(reg), DB_LEN_STR(reg));
			continue;
		}
		if (!STANDALONE(reg) || !(FILE_INFO(reg)->grabbed_access_sem))
		{
			error = TRUE;
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_MUSTANDALONE, 2, DB_LEN_STR(reg));
			util_out_print("Region !AD : Failed to get standalone access (!AD).",
					TRUE, REG_LEN_STR(reg), DB_LEN_STR(reg));
			continue;
		}
		if (was_asyncio_enabled)
		{
			util_out_print("Region !AD : Disabling ASYNCIO for the duration of upgrade", TRUE, REG_LEN_STR(reg));
			if (NULL == pvt_csd)
			{
				pvt_csd = (sgmnt_data *)malloc(ROUND_UP(SIZEOF(sgmnt_data), DISK_BLOCK_SIZE));
				pvt_csd->read_only = FALSE;
			}
			/* Adapted from mupip_set_file.c */
			if (FD_INVALID == (fd = OPEN(fn, O_RDWR)))	/* udi not available so OPENFILE_DB not used */
			{
				save_errno = errno;
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_DBFILOPERR, 2, LEN_AND_STR(fn), save_errno);
				continue;
			}
#			ifdef __MVS__
			if (-1 == gtm_zos_tag_to_policy(fd, TAG_BINARY, &realfiletag))
				TAG_POLICY_GTM_PUTMSG(fn, realfiletag, TAG_BINARY, errno);
#			endif
			LSEEKREAD(fd, 0, pvt_csd, SIZEOF(sgmnt_data), status);
			if (0 == memcmp(pvt_csd->label, V6_GDS_LABEL, GDS_LABEL_SZ -1))
				db_header_upconv(pvt_csd);
			if (0 != status)
			{
				save_errno = errno;
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_DBFILOPERR, 2, LEN_AND_STR(fn), save_errno);
				if (-1 != status)
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_DBRDERR, 2, fn_len, fn);
				else
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_DBPREMATEOF, 2, fn_len, fn);
				RELEASE_ACCESS_SEMAPHORE_AND_RUNDOWN(gv_cur_region, error);
				continue;
			}
			FILE_INFO(gv_cur_region)->fd_opened_with_o_direct = FALSE;
			pvt_csd->asyncio = FALSE;
			if (0 == memcmp(pvt_csd->label, V6_GDS_LABEL, GDS_LABEL_SZ - 1))
				db_header_dwnconv(pvt_csd);
			DB_LSEEKWRITE(NULL,((unix_db_info *)NULL),NULL,fd,0,pvt_csd,SIZEOF(sgmnt_data),status);
			if (0 != status)
			{
				save_errno = errno;
				errptr = (char *)STRERROR(save_errno);
				util_out_print("write : !AZ", TRUE, errptr);
				util_out_print("Error writing header of file", TRUE);
				util_out_print("Database file !AD not changed: ", TRUE, fn_len, fn);
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_DBRDERR, 2, fn_len, fn);
			}
			CLOSEFILE_RESET(fd, rc);
			db_ipcs_reset(reg);
			if (!STANDALONE(reg) || !(FILE_INFO(reg)->grabbed_access_sem))
			{
				error = TRUE;
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_MUSTANDALONE, 2, DB_LEN_STR(reg));
				util_out_print("Region !AD : Failed to get standalone access (!AD).",
						TRUE, REG_LEN_STR(reg), DB_LEN_STR(reg));
				continue;
			}
		}
		/* Reopen the region now that we have standalone access */
		gvcst_init(reg);
		change_reg();
		/* Setup for upgrade - the following will handle any version conflicts */
		status = verify_db_format_change_request(reg, GDSV6p, "MUPIP UPGRADE");
		if (SS_NORMAL != status)
		{
			error = TRUE;							/* move on to the next region */
			RELEASE_ACCESS_SEMAPHORE_AND_RUNDOWN(gv_cur_region, error);
			continue;
		}
		csd = cs_data;
		assert(!csd->asyncio);
		blk_size = csd->blk_size;
		util_out_print("Region !AD : MUPIP MASTERMAP UPGRADE started (!AD)", TRUE, REG_LEN_STR(reg), DB_LEN_STR(reg));
		blocks_needed = csd->trans_hist.total_blks - csd->trans_hist.free_blocks;
		blocks_needed -= (csd->trans_hist.total_blks / BLKS_PER_LMAP);	/* Subtract LMAPs from the count */
		/* The largest possible key size is limited by block size. The block must be able to fit a pointer with the longest
		 * possible key size plus a star-record. This limitation comes from the index blocks and not the data blocks.
		 */
		lcl_max_key_size = blk_size - sizeof(blk_hdr) - sizeof(rec_hdr) - SIZEOF_BLK_ID(TRUE) - bstar_rec_size(TRUE);
		if (FALSE == (was_crit = csa->now_crit)) /* WARNING assigment */
			grab_crit(reg, WS_1);
		if (JNL_ENABLED(csd))
		{
			SET_GBL_JREC_TIME;	/* needed for jnl_ensure_open, jnl_write_pini and jnl_write_aimg_rec */
			jpc = csa->jnl;
			jbp = jpc->jnl_buff;
			/* Before writing to jnlfile, adjust jgbl.gbl_jrec_time if needed to maintain time order of jnl records.
			 * This needs to be done BEFORE the jnl_ensure_open as that could write journal records
			 * (if it decides to switch to a new journal file)
			 */
			ADJUST_GBL_JREC_TIME(jgbl, jbp);
			jnl_status = jnl_ensure_open(reg, csa);
			if (0 == jnl_status)
			{
				save_inctn_opcode = inctn_opcode;
				inctn_opcode = inctn_db_format_change;
				inctn_detail.blks2upgrd_struct.blks_to_upgrd_delta = csd->blks_to_upgrd;
				if (0 == jpc->pini_addr)
					jnl_write_pini(csa);
				jnl_write_inctn_rec(csa);
				inctn_opcode = save_inctn_opcode;
			} else
				gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(csd), DB_LEN_STR(reg));
		}
		if (FALSE == was_crit)
			rel_crit(reg);
		if (3 >= blocks_needed)
		{	/* Completely empty DB */
			if (dba_mm == reg->dyn.addr->acc_meth)
			{	/* MM: Re-align cached GV targets because they weren't setup for gv_cur_region */
				for (gvt = gv_target_list; NULL != gvt; gvt = gvt->next_gvnh)
				{
					gvt->clue.end = 0;	/* Resetting to zero forces a refetch */
					gvt->gd_csa = cs_addrs;	/* UPGRADE overrides all names to be in the current region */
				}
			}
			util_out_print("Region !AD : WARNING, region is currently empty. MUPIP UPGRADE will adjust the region",
			       TRUE, REG_LEN_STR(reg));
			util_out_print("Region !AD : Please considering recreating the region with V7 for optimal results",
			       TRUE, REG_LEN_STR(reg));
			/* Start-up conditions */
			mu_upgrade_in_prog = MUPIP_UPGRADE_IN_PROGRESS;
			csd->fully_upgraded = FALSE;
			/* Extend the DB to accomodate the larger master map. Asking for 2x SVBN blocks is quick'n'dirty math */
			if (SS_NORMAL != (status = upgrade_extend(START_VBN_CURRENT << 1, reg)))	/* WARNING assignment */
			{	/* extension failed, try the next region */
				mu_upgrade_in_prog = MUPIP_UPGRADE_OFF;
				util_out_print("Region !AD : Error while attempting to make room for enlarged master bitmap.",
				       FALSE, REG_LEN_STR(reg));
				util_out_print("  Moving onto next region.",TRUE);
				error = TRUE;
				RELEASE_ACCESS_SEMAPHORE_AND_RUNDOWN(gv_cur_region, error);
				continue;
			}
			if (FALSE == (was_crit = csa->now_crit)) /* WARNING assigment */
				grab_crit(reg, WS_1);
			/* Adjust starting VBN and block counts accordingly. See mu_upgrade_bmm for the explanation of the
			 * calculations here */
			blks_in_way = START_VBN_CURRENT - csd->start_vbn;
			blks_in_way = ROUND_UP2((blks_in_way / (blk_size / DISK_BLOCK_SIZE)), BLKS_PER_LMAP);
			bmls_to_work = blks_in_way / BLKS_PER_LMAP;		/* on a small DB this is overkill, but simpler */
			bmls_to_work = ROUND_UP2(bmls_to_work, BITS_PER_UCHAR / MASTER_MAP_BITS_PER_LMAP);
			blks_in_way = bmls_to_work * BLKS_PER_LMAP;
			assert((csd->start_vbn + (blks_in_way * blk_size) / DISK_BLOCK_SIZE) >= START_VBN_CURRENT);
			/* Adjust block information */
			old_vbn = csd->start_vbn;
			csd->start_vbn += (blks_in_way * blk_size) / DISK_BLOCK_SIZE;	/* upgraded file has irregular start_vbn */
			csd->trans_hist.total_blks -= blks_in_way;
			csd->trans_hist.free_blocks -= (blks_in_way - bmls_to_work);	/* lost space less bmls which remain busy */
			/* Adjust master bit map (aka bmm)
			 * - Set the new length
			 * - Allocate an aligned buffer for new bmm (in case AIO), see DIO_BUFF_EXPAND_IF_NEEDED
			 * - Set all to free; because the DB is empty, this code does not need to retain prior block allocation
			 */
			csd->master_map_len = MASTER_MAP_SIZE_DFLT;
			udi = FILE_INFO(reg);
			/* Allocate an aligned buffer for new bmm (in case AIO), see DIO_BUFF_EXPAND_IF_NEEDED */
			new_bmm_size = ROUND_UP(MASTER_MAP_SIZE_DFLT, cs_data->blk_size) + OS_PAGE_SIZE;
			bml_buff = malloc(new_bmm_size);
			bmm_base = (sm_uc_ptr_t)ROUND_UP2((sm_long_t)bml_buff, OS_PAGE_SIZE);
			assert(OS_PAGE_SIZE >= (bmm_base - bml_buff));
			memset(bmm_base, BMP_EIGHT_BLKS_FREE, MASTER_MAP_SIZE_DFLT); 	      /* Initialize entire bmm to FREE */
			DB_LSEEKWRITE(csa, udi, udi->fn, udi->fd, SGMNT_HDR_LEN, bmm_base,    /* Write new BMM after file header */
					MASTER_MAP_SIZE_DFLT, status);
			assert(SS_NORMAL == status);
			free(bml_buff);								/* A little early in case of err */
			/* Establish new root blocks */
			csd->desired_db_format = GDSV7m;
			mucblkini(GDSV7m);			/* Recreate the DB with V7m directory tree */
			/* At this point the file header claims all V6ish settings such that there is a V6p database
			 * certified for upgrade to V7m in spite of there being no actual datablocks. Emit a warning
			 * about the wastage of space for no good reason
			 */
			util_out_print("Region !AD : Region is currently empty. Reinitializing it results in a loss of space.",
			       TRUE, REG_LEN_STR(reg));
			util_out_print("Region !AD : Please considering recreating the region with V7 for optimal results",
			       TRUE, REG_LEN_STR(reg));
			/* Finalize header adjustments */
			csd->certified_for_upgrade_to = GDSV7m;
			csd->blks_to_upgrd = csd->tn_upgrd_blks_0 = csd->reorg_upgrd_dwngrd_restart_block =
				csd->blks_to_upgrd_subzero_error =  0;
			/* Calculate free (wasted?) space between HDR and the new starting VBN */
			csd->free_space = BLK_ZERO_OFF(csd->start_vbn) - SIZEOF_FILE_HDR(csd);
			memset(csd->reorg_restart_key, 0, MAX_MIDENT_LEN + 1);
			db_header_dwnconv(csd);				/* revert the file header to V6 format so we can save it */
			db_header_upconv(csd);				/* finish transition to new header */
			csd->fully_upgraded = TRUE;			/* Since it is V7 */
			MEMCPY_LIT(csd->label, GDS_LABEL);		/* Change to V7 label, fully upgraded */
			csd->minor_dbver = GDSMVCURR;			/* Raise the DB minor version */
#ifdef _AIX
			if (dba_mm == reg->dyn.addr->acc_meth)
				wcs_mm_recover(reg, csd->start_vbn - old_vbn);
#endif
			wcs_flu(WCSFLU_IN_COMMIT | WCSFLU_FLUSH_HDR | WCSFLU_CLEAN_DBSYNC);
			if (FALSE == was_crit)
				rel_crit(reg);
			mu_upgrade_in_prog = MUPIP_UPGRADE_OFF;
		} else
		{	/* DB with something in it */
			/* Make room for the enlarged master bitmap - calculate space needed to hold larger (64-bit) pointers */
			csd->max_rec = (blk_size - SIZEOF(blk_hdr)) /* max records of smallest (1 char) keys with 2 delimiters */
				/ (SIZEOF(rec_hdr) + (3 * SIZEOF(KEY_DELIMITER)) + SIZEOF_BLK_ID(BLKID_64));
			csd->i_reserved_bytes = csd->max_rec * (SIZEOF_BLK_ID(BLKID_64) - SIZEOF_BLK_ID(BLKID_32));
			blocks_needed = blocks_needed >> 1; /* index blks cnt guess */
			blocks_needed = DIVIDE_ROUND_UP(blocks_needed * csd->i_reserved_bytes, blk_size);
			jnl_state = csd->jnl_state;					/* save jnl and repl states for restore */
			repl_state = csd->repl_state;
			csd->repl_state = csa->repl_state = repl_closed;		/* standalone gap in jnl and repl */
			csd->jnl_state = csa->jnl_state = jnl_notallowed;
			status = mu_upgrade_bmm(reg, blocks_needed);			/* main standane upgrade */
			csd->jnl_state = csa->jnl_state = jnl_state;			/* restore jnl and repl states */
			csd->repl_state = csa->repl_state = repl_state;
			if (SS_NORMAL != status)
			{	/* enlarging master bitmap failed; release standalone access, print status & go to next region */
				mu_upgrade_in_prog = MUPIP_UPGRADE_OFF;
				util_out_print("Region !AD : Error while attempting to make room for enlarged master bitmap.",
				       FALSE, REG_LEN_STR(reg));
				util_out_print("  Moving onto next region.",TRUE);
				error = TRUE;
				RELEASE_ACCESS_SEMAPHORE_AND_RUNDOWN(gv_cur_region, error);
				continue;
			}
		}
		if (JNL_ENABLED(csd))
		{
			if (FALSE == (was_crit = csa->now_crit)) /* WARNING assigment */
				grab_crit(reg, WS_1);
			if (csd->jnl_file_len)
				cre_jnl_file_intrpt_rename(csa);
			sts = set_jnl_file_close();
			assert(SS_NORMAL == sts);	/* because we should have done jnl_ensure_open already
							 * in which case set_jnl_file_close has no way of erroring out.
							 */
			mu_upgrade_in_prog = MUPIP_UPGRADE_IN_PROGRESS;
			sts = jnl_file_open_switch(reg, 0, buff, OUT_BUFF_SIZE);	/* Break prevlink */
			mu_upgrade_in_prog = MUPIP_UPGRADE_OFF;
			if (sts)
			{
				jpc = csa->jnl;
				if (NOJNL != jpc->channel)
					JNL_FD_CLOSE(jpc->channel, close_res);	/* sets jpc->channel to NOJNL */
				assert(NOJNL == jpc->channel);
				jnl_send_oper(jpc, sts);
			}
			if (FALSE == was_crit)
				rel_crit(reg);
		}
		if (csd->max_key_size > lcl_max_key_size)
		{
			util_out_print("Region !AD : WARNING The maximum key size for !AD is !UL",
					TRUE, REG_LEN_STR(reg), DB_LEN_STR(reg), csd->max_key_size);
			util_out_print("Region !AD : WARNING This exceeds the largest possible size for a V7 database",
					FALSE, REG_LEN_STR(reg));
			util_out_print("with the current block size !UL.",
					TRUE, blk_size);
			util_out_print("Region !AD : WARNING Please run MUPIP INTEG on !AD to ensure all keys are less than !UL",
					TRUE, REG_LEN_STR(reg), DB_LEN_STR(reg), lcl_max_key_size);
			csd->max_key_size = lcl_max_key_size;
			csd->maxkeysz_assured = FALSE;
		}
		if (was_asyncio_enabled)
		{
			util_out_print("Region !AD : Restoring ASYNCIO setting fo !AD", TRUE, REG_LEN_STR(reg), DB_LEN_STR(reg));
			csa->hdr->asyncio = TRUE;
		}
		RELEASE_ACCESS_SEMAPHORE_AND_RUNDOWN(gv_cur_region, error);
		util_out_print("Region !AD : MUPIP MASTERMAP UPGRADE of !AD completed", TRUE, REG_LEN_STR(reg), DB_LEN_STR(reg));
	}
	if (file)
		mu_gv_cur_reg_free();
	mupip_exit(error ? ERR_MUNOFINISH : SS_NORMAL);
	return;
}
