/****************************************************************
 *								*
 * Copyright (c) 2005-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2023 YottaDB LLC and/or its subsidiaries.	*
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
#include "mu_outofband_setup.h"
#include "mu_all_version_standalone.h"

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

<<<<<<< HEAD
GBLREF	boolean_t		exit_handler_active;
GBLREF	boolean_t		exit_handler_complete;

LITREF  char            	ydb_release_name[];
LITREF  int4           		ydb_release_name_len;
=======
#define V6TOV7UPGRADEABLE 1
>>>>>>> f9ca5ad6 (GT.M V7.1-000)

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

<<<<<<< HEAD
#define BIG_GVNAME		"zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz"
#define SML_GVNAME		"%"

static sem_info	*sem_inf;
static gtm_int8	tot_dt, tot_levl_cnt, tot_splt_cnt;

void mupip_upgrade(void)
{
	boolean_t	error = FALSE, got_standalone, long_blk_id, mastermap;
	block_id	blk_ptr, root_blk_id;
	gd_region	*reg;
	int		split_blks_added, split_levels_added;
	int4		blk_size, idx, key_cmpc, key_len, level, l_tries, rec_sz, status;
	mident		root_name;
	mval		big_gvname = DEFINE_MVAL_STRING(MV_STR | MV_NUM_APPROX, 0, 0, SIZEOF(BIG_GVNAME) - 1,
				(char *)BIG_GVNAME, 0, 0);
	mval		sml_gvname = DEFINE_MVAL_STRING(MV_STR | MV_NUM_APPROX, 0, 0, SIZEOF(SML_GVNAME) - 1,
							(char *)SML_GVNAME, 0, 0);
	mval		*v;
	size_t		blocks_needed;
	sgmnt_addrs	*csa;
	sgmnt_data	*csd;
	sm_uc_ptr_t	blkBase, blkEnd, new_file_header, recBase;
	srch_blk_status	dirHist;
	srch_hist	*lft_history;
	tp_region	*rptr;
	unix_db_info	*udi;
	unsigned char	first_buff[MAX_KEY_SZ + 1], key_buff[MAX_KEY_SZ + 1];
#	ifdef notnow
	mval		first_key = DEFINE_MVAL_STRING(MV_STR | MV_NUM_APPROX,  0, 0, 0, (char *)first_buff, 0, 0);
#	endif
=======
void mupip_upgrade(void)
{
	boolean_t		error, file, region, was_asyncio_enabled, was_crit;
	block_id		blks_in_way, old_vbn;
	char			buff[OUT_BUFF_SIZE], *errptr, fn[MAX_FN_LEN + 1];
	enum jnl_state_codes	jnl_state;
	enum repl_state_codes	repl_state;
	gd_region		*reg;
	gd_segment		*seg;
	gv_namehead		*gvt;
	inctn_opcode_t		save_inctn_opcode;
	int			exit_status, fd, close_res, rc, save_errno, split_blks_added, split_levels_added;
	int4			blk_size, bmls_to_work, new_bmm_size, status;
	jnl_buffer_ptr_t	jbp;
	jnl_private_control	*jpc;
	size_t			blocks_needed, fn_len;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd, pvt_csd = NULL;
	sm_uc_ptr_t		bmm_base, bml_buff;
	tp_region		*rptr, single;
	uint4			jnl_status, sts;
	unix_db_info		*udi;
>>>>>>> f9ca5ad6 (GT.M V7.1-000)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Initialization */
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
	gvinit();					/* initialize gd_header (needed by the later call to mu_getlst) and gv_keysize */
	if ((file == region) && (TRUE == file))
		mupip_exit(ERR_MUQUALINCOMP);
	else if (file)
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
		mu_getlst("WHAT", SIZEOF(tp_region));
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
<<<<<<< HEAD
		gvcst_init(reg);
		change_reg();
		csa = cs_addrs;
		csd = cs_data;
		if (0 != memcmp(csd->label, V6_GDS_LABEL, GDS_LABEL_SZ - 1))
		{	/* This is not a V6 region so it cannot be upgraded */
			error = TRUE;
			gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_TEXT, 2,
					LEN_AND_LIT("Non-V6.x database cannot be upgraded"));
			continue; /* move onto next region */
		}
		if (RDBF_STATSDB_MASK == csd->reservedDBFlags)
		{	/* TODO: statsDB need upgrades */
			error = TRUE;
			gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_STATSDBNOTSUPP, 2, DB_LEN_STR(reg));
			continue; /* move onto next region */
		}
		mastermap = FALSE;	/* to silence [-Wuninitialized] warning */
		if (mastermap)
		{
			/* Region can be upgraded - run it down and get standalone TODO: abuse rollback flags to avoid standalone */
			if (EXIT_NRM != gds_rundown(CLEANUP_UDI_FALSE))
			{	/* Failed to rundown the DB, so we can't get standalone access */
				error = TRUE;
				util_out_print("Region !AD : Failed to rundown the database in order to get standalone access.",
					       TRUE, REG_LEN_STR(reg));
				continue;
			}
			got_standalone = STANDALONE(reg);
			if (!got_standalone || !(FILE_INFO(reg)->grabbed_access_sem))
			{
				csa = &FILE_INFO(reg)->s_addrs;
				error = TRUE;
				gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_MUSTANDALONE, 2, DB_LEN_STR(reg));
				util_out_print("Region !AD : Failed to get standalone access.",
					       TRUE, REG_LEN_STR(reg));
				continue;
			}
			/* Reopen the region now that we have standalone access */
			gvcst_init(reg);
			change_reg();
			csd = cs_data;
			blk_size = csd->blk_size;
			util_out_print("!/Region !AD : MUPIP MASTERMAP UPGRADE started", TRUE, REG_LEN_STR(reg));
			/* Make room for the enlarged master bitmap */
			/* TODO: when to make journal file switch after header calcs/updates */
			/* calculate speace needed to larger (64-bit) pointers */
			csd->max_rec = (blk_size - SIZEOF(blk_hdr)) /* max records of smallest (1 char) keys with 2 delimiters */
				/ (SIZEOF(rec_hdr) + (3 * SIZEOF(KEY_DELIMITER)) + SIZEOF_BLK_ID(BLKID_64));
			csd->i_reserved_bytes = csd->max_rec * (SIZEOF_BLK_ID(BLKID_64) - SIZEOF_BLK_ID(BLKID_32));
			blocks_needed = (csd->trans_hist.total_blks - csd->trans_hist.free_blocks) >> 3; /* index blks cnt guess */
			blocks_needed = DIVIDE_ROUND_UP(blocks_needed * csd->i_reserved_bytes, blk_size);
			status = mu_upgrade_bmm(reg, blocks_needed);
			if (SS_NORMAL != status)
			{	/* There was an error while enlarging the master bitmap so cancel upgrade */
				/* Release standalone access */
#ifdef notnow	/* TODO: why are these suppressed? */
				assert(FILE_INFO(gv_cur_region)->grabbed_access_sem);
				db_ipcs_reset(gv_cur_region);
#endif
				/* Print status message and move onto next region */
				util_out_print("Region !AD : Error while attempting to make room for enlarged master bitmap.",
					       FALSE, REG_LEN_STR(reg));
				util_out_print("  Moving onto next region.",TRUE);
				error = TRUE;
				continue;
			}
#ifdef notnow
			/* Now that DB has been upgraded to V7 rundown and reopen the DB inorder to flush the block cache */
			mu_rndwn_file(gv_cur_region, FALSE);
			assert(FILE_INFO(gv_cur_region)->grabbed_access_sem);
			db_ipcs_reset(gv_cur_region);
			if (EXIT_NRM != gds_rundown(CLEANUP_UDI_FALSE))
=======
		/* Override name map so that all names map to the current region */
		remap_globals_to_one_region(gd_header, gv_cur_region);
		gvcst_init(reg, NULL);
		change_reg();
		csa = cs_addrs;
		if (csa->hdr->read_only)
		{
			error = TRUE;
			util_out_print("Region !AD : Region is read-only, skipping database !AD",
			       TRUE, REG_LEN_STR(reg), DB_LEN_STR(reg));
			if (EXIT_NRM != gds_rundown(CLEANUP_UDI_TRUE))
>>>>>>> f9ca5ad6 (GT.M V7.1-000)
			{	/* Failed to rundown the DB */
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_DBRNDWN, 2, DB_LEN_STR(gv_cur_region));
				util_out_print("Region !AD : Failed to rundown the database (!AD) in order to get standalone access.",
				       TRUE, REG_LEN_STR(reg), DB_LEN_STR(reg));
			}
<<<<<<< HEAD
			gvcst_init(reg);
			change_reg();
			csa = cs_addrs;
			csd = cs_data;
			/* Clean up */
			free(new_file_header);
			new_file_header = NULL;
			assert(FILE_INFO(gv_cur_region)->grabbed_access_sem);
			db_ipcs_reset(gv_cur_region);
#endif
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_MUPGRDSUCC, 6, DB_LEN_STR(reg), RTS_ERROR_LITERAL("upgraded"),
				       ydb_release_name_len, ydb_release_name);
		} else
		{
			if (GDSV7m != csd->desired_db_format)
			{	/* This is not a region with an upgraded master map, so it cannot be upgraded */
				error = TRUE;
				gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_TEXT, 2,
					LEN_AND_LIT("Database cannot be fully upgraded until it has had a mastermap upgrade"));
				break; /* move onto next region */
			}
			mu_reorg_upgrd_dwngrd_in_prog = TRUE;
			blk_size = csd->blk_size;
			/* TODO ability to restart ??? */
			util_out_print("!/Region !AD : MUPIP UPGRADE started", TRUE, REG_LEN_STR(reg));
			v = (mval *)&sml_gvname;
			do	/* decend to the right end of the directory tree and move  through the gvt pointers */
			{
				op_gvname(1, v);
				op_gvorder(v);	/* TODO special code for ^% - see if the first try gets the first record? */
				if (0 == v->str.len)
					break;
				root_blk_id = gv_target->next_gvnh->hist.h[0].blk_num;
				blkBase = gv_target->next_gvnh->hist.h[0].buffaddr;
				blkEnd = blkBase + ((blk_hdr_ptr_t)blkBase)->bsiz;
				for (recBase = blkBase + SIZEOF(blk_hdr), l_tries = 0; recBase < blkEnd;)
				{	/* read the block and use each record to upgrade a gvt */
					if (NULL == (dirHist.buffaddr = t_qread(root_blk_id, (sm_int_ptr_t)&dirHist.cycle,
						&dirHist.cr)))
					{
						l_tries++;
						continue;
					}
#ifdef notnow
					if (blkBase != dirHist.buffaddr)
					{	/* block moved, so restart the processing TODO: risk of an indefinite loop? */
						blkBase = dirHist.buffaddr;
						blkEnd = blkBase + ((blk_hdr_ptr_t)blkBase)->bsiz;
						recBase = blkBase + SIZEOF(blk_hdr);
						l_tries++;
						continue;
					}
#endif
					level = 0;	/* to silence [-Wuninitialized] warning */
					if (cdb_sc_normal
						!= read_record(&rec_sz, &key_cmpc, &key_len, key_buff, level, &dirHist, recBase))
					{	/* failed to parse record */
						l_tries++;
						continue;
					}
					long_blk_id = BLKID_64;	/* to silence [-Wuninitialized] warning */
					READ_BLK_ID(long_blk_id, &blk_ptr, recBase + SIZEOF(rec_hdr) + key_len);
					if ((0 == blk_ptr) || (csa->ti->total_blks < blk_ptr))
					{	/* block_id out of range */
						l_tries++;
						continue;
					}
					root_name.addr = (char *)key_buff;
					root_name.len = key_len;
#ifdef notnow
					if (0 == first_key.str.len)
					{
						first_key.str.len = key_len;
						memcpy(first_key.str.addr, root_name.addr, key_len);
					}
#endif
					if (cdb_sc_normal != upgrade_gvt(blk_ptr, blk_size, reg, &root_name))
					{	/* gvt_upgrade had a problem; WARNING assign above */
						l_tries++;
						continue;
					}
					recBase += rec_sz;
				}
				v->str = root_name;
			} while (0 != v->str.len);
			mu_reorg_upgrd_dwngrd_in_prog = FALSE;
			/* report counts */
		} while (mu_reorg_upgrd_dwngrd_in_prog);
	}
	if (error)
		mupip_exit(ERR_MUNOFINISH);
	else
		mupip_exit(SS_NORMAL);
}
enum cdb_sc upgrade_gvt(block_id curr_blk, int4 blk_size, gd_region *reg, mstr *root)
{	/* work down the left edge of the gvt and process each level repetatively until there are no more splits */
	blk_hdr_ptr_t	rootBase;
	blk_segment	*bs1, *bs_ptr;
	block_id	blk_pter;
	boolean_t	long_blk_id, was_crit;
	gvnh_reg_t	*gvnh_reg;
	int		blk_sz, key_cmpc, key_len, level, rec_sz;
	int4		status;
	mstr		name;
	sgmnt_addrs	*csa;
	sm_uc_ptr_t	blkBase, blkEnd, recBase;
	srch_blk_status	dirHist;
	srch_hist	lft_history;
	unsigned char	first_buff[MAX_KEY_SZ + 1], key_buff[MAX_KEY_SZ + 1];
	mval		first_key = DEFINE_MVAL_STRING(MV_STR | MV_NUM_APPROX,  0, 0, 0, (char *)first_buff, 0, 0);

	DBGUPGRADE(util_out_print("!/Region !AD: Global: !AD started at block: @x!@XQ", TRUE, REG_LEN_STR(reg), root->len,
		root->addr, &curr_blk));
	return cdb_sc_normal;
	assert(!update_trans && !need_kip_incr);
	csa = cs_addrs;
	dirHist.blk_num = curr_blk;
	update_trans = UPDTRNS_DB_UPDATED_MASK;
	grab_crit(reg, WS_64);
	csa->hold_onto_crit = TRUE;
	t_begin_crit(ERR_MUNOUPGRD);
	if (NULL == (dirHist.buffaddr = t_qread(dirHist.blk_num, (sm_int_ptr_t)&dirHist.cycle, &dirHist.cr)))
	{	/* because we, somewhat nastily, have grabbed control of the DB this should not happen */
		status = rdfail_detail;
		assert(cdb_sc_normal == status);
		csa->hold_onto_crit = FALSE;
		t_abort(gv_cur_region, csa);
		return status;
	}
	blkBase = dirHist.buffaddr;
	level = ((blk_hdr_ptr_t)(blkBase))->levl;
	assert(level);
	if ((cdb_sc_normal) != (status = upgrade_idx_block(dirHist.blk_num, blk_size, reg, root)))
	{	/* Failed to upgrade the indicated block - this should not happen */
		assert(cdb_sc_normal == status);
		csa->hold_onto_crit = FALSE;
		t_abort(gv_cur_region, csa);
		return status;
	}
	assert(!csa->now_crit & !csa->hold_onto_crit);
	blkEnd = blkBase + ((blk_hdr_ptr_t)blkBase)->bsiz;
	for (blkEnd = blkBase + ((blk_hdr_ptr_t)blkBase)->bsiz; --level; )
	{
		first_key.str.len = 0;
		for (recBase = blkBase + SIZEOF(blk_hdr); recBase < blkEnd; )
		{
			if (!(was_crit = csa->now_crit))						/* WARNING assignment */
				grab_crit(reg, WS_64);
			csa->hold_onto_crit = TRUE;
			t_begin_crit(ERR_MUNOUPGRD);
			if (NULL == (dirHist.buffaddr = t_qread(dirHist.blk_num, (sm_int_ptr_t)&dirHist.cycle, &dirHist.cr)))
			{	/* because we, somewhat nastily, have crit on the DB this should not happen; WARNING assign above */
				status = rdfail_detail;
				assert(cdb_sc_normal == status);
				csa->hold_onto_crit = FALSE;
				t_abort(gv_cur_region, csa);
				return status;
			}
			if (blkBase != dirHist.buffaddr)
			{	/* block moved, so restart the processing TODO: what's the risk of an indefinite loop? */
				blkBase = dirHist.buffaddr;
				blkEnd = blkBase + ((blk_hdr_ptr_t)blkBase)->bsiz;
				recBase = blkBase + SIZEOF(blk_hdr);
				continue;
			}
			assert(GDSV7m == ((blk_hdr_ptr_t)blkBase)->bver);
			level = ((blk_hdr_ptr_t)blkBase)->levl;	/* might have increased due to split propagation */
			if (BSTAR_REC_SIZE_64 == (rec_sz = ((rec_hdr_ptr_t)recBase)->rsiz))		/* WARNING assignment*/
			{	/* *-key record - name must carry over TODO does it need a "push?" */
				assert(((recBase + rec_sz) == blkEnd) && (1 == level));
				if (!first_key.str.len)
				{	/* global has been KILL'd */
					first_key.str.len = key_len = root->len;
					first_key.str.addr = (char *)root->addr;
					memcpy(key_buff, root->addr, key_len);
				}
				READ_BLK_ID(BLKID_64, &blk_pter, recBase + SIZEOF(rec_hdr));
				status = cdb_sc_normal;
			} else if (cdb_sc_normal == (status = read_record(&rec_sz, &key_cmpc, &key_len, key_buff, level, &dirHist,
					recBase)))
				READ_BLK_ID(long_blk_id, &blk_pter, recBase + SIZEOF(rec_hdr) + key_len);
			else
			{	/* Failed to parse the record - this should not happen */
				assert(cdb_sc_normal == status);
				csa->hold_onto_crit = FALSE;
				t_abort(gv_cur_region, csa);
				return status;
			}
			if ((0 == blk_pter) || (csa->ti->total_blks < blk_pter))
			{	/* block_id out of range - this should not happen */
				status = cdb_sc_blknumerr;
				assert(cdb_sc_normal == status);
				csa->hold_onto_crit = FALSE;
				t_abort(gv_cur_region, csa);
				return status;
			}
			name.addr = (char *)key_buff;
			name.len = key_len;
			if ((cdb_sc_normal) != (status = upgrade_idx_block(blk_pter, blk_size, reg, &name)))
			{	/* Failed to upgrade the indicated block - this should not happen */
				assert(cdb_sc_normal == status);
				csa->hold_onto_crit = FALSE;
				t_abort(gv_cur_region, csa); /* TODO: is this loop safe?; see t_abort for idea */
				continue;
			}
			assert(!csa->now_crit & !csa->hold_onto_crit);			/* TODO: pause to avoid greed? */
			if (0 == first_key.str.len)
			{
				first_key.str.len = key_len;
				memcpy(first_key.str.addr, name.addr, name.len);
			}
			recBase += rec_sz;
		}
		assert(first_key.str.len);
		op_gvname(1, &first_key);
		op_zprevious(&first_key);
		if (0 != first_key.str.len)
		{	/* setup and continue */
			dirHist.blk_num = lft_history.h[level].blk_num;
			continue;
		}
		dirHist.blk_num = lft_history.h[level -1].blk_num;		/* drop a level. setup and continue */
	}
	return status;
}

/******************************************************************************************
 * This recursively traverses a global variable tree and and upgrades the pointers in index
 * blocks, which likely entails block splitting. It works down levels, because splits that
 * add levels then create new blocks in the desired format to match the upgraded block
 * being split, works index blocks from left to right, because blocks split to the left.
 * but works from right to left within a block because of key compression.
 * It expects to be called in crit with hold_onto_crit and releases crit when complete.
 * As mentioned above, it upgrages all the pointers from 32-bit to 64-bit format; which
 * is unlikely to be needed in the event of any subsequent master map extentions, nor
 * address the needs of any future block format changes.
 *
 * Input Parameters:
 * 	curr_blk identifies an index block
* 	blk_size gives the block size for the region
 * 	reg points to the base structure for the region
 * 	name points to an mstr containing key text
 * Output Parameters:
 * 	(enum_cdb_sc) returns cdb_sc_normal which the code expects or a retry code
 ******************************************************************************************/
enum cdb_sc upgrade_idx_block(block_id curr_blk, int4 blk_size, gd_region *reg, mstr *name)
{
	blk_segment		*bs1, *bs_ptr;
	block_id		blk_pter;
	boolean_t		long_blk_id;
	enum db_ver		blk_ver;
	gvnh_reg_t		*gvnh_reg;
	int			blk_seg_cnt, level, split_blks_added, split_levels_added, key_cmpc, key_len, new_blk_sz, num_recs,
				rec_sz, space_need, v7_rec_sz;
	int4			status;
	mname_entry		gvname;
	sm_uc_ptr_t		blkBase, blkEnd, recBase, v7bp, v7recBase;
	srch_blk_status		dirHist, *left_blk_status;
	srch_hist		left_hist;
	unsigned char		key_buff[MAX_KEY_SZ + 1];

	/* TODO: ensure t_retry paths are sound */
	assert(cs_addrs->now_crit & cs_addrs->hold_onto_crit);
	util_out_print("adjusting index block @x!@XQ keyed with !AD", TRUE, &curr_blk, name->len, name->addr);
	dirHist.blk_num = curr_blk;
	if (NULL == (dirHist.buffaddr = t_qread(dirHist.blk_num, (sm_int_ptr_t)&dirHist.cycle, &dirHist.cr)))	/* WARNING assign */
	{
		status = rdfail_detail;
		assert(cdb_sc_normal == status);
		return status;
	}
	blkBase = dirHist.buffaddr;
	if (GDSV7m == ((blk_hdr_ptr_t)blkBase)->bver)
	{
		cs_addrs->hold_onto_crit = FALSE;
		t_abort(gv_cur_region, cs_addrs);						/* do crit and other cleanup */
		cs_data->blks_to_upgrd--;
		return cdb_sc_normal;
	}
	new_blk_sz = ((blk_hdr_ptr_t)blkBase)->bsiz;
	blkEnd = blkBase + new_blk_sz;
	dirHist.level = level = ((blk_hdr_ptr_t)blkBase)->levl;
	assert(level);
	long_blk_id = IS_64_BLK_ID(blkBase);
	assert(FALSE == long_blk_id);
	blk_ver = ((blk_hdr_ptr_t)blkBase)->bver;
	PRO_ONLY(UNUSED(blk_ver));
	recBase = blkBase + SIZEOF(blk_hdr);
	if (0 == name->len)
	{	/* TODO: deal with *-key search */
		assert((SIZEOF(blk_hdr) + SIZEOF(rec_hdr) + SIZEOF_BLK_ID(FALSE)) == new_blk_sz);
	} else
	{
		assert(TRUE);
	}
	/* check how much space is need to upgrade the block and whether the block will need to be split */
	while (recBase < blkEnd)
	{	/* iterate through index block counting records */
		if (cdb_sc_starrecord == (status = read_record(&rec_sz, &key_cmpc, &key_len, key_buff, level, &dirHist, recBase)))
		{	/* WARNING assignment above */
			assert((((rec_hdr_ptr_t)recBase)->rsiz + recBase == blkEnd) && (0 != level));
			memcpy(gv_target->alt_hist, &(gv_target->hist), SIZEOF(srch_hist));
			key_len = 0;
		} else if (cdb_sc_normal != status)
		{	/* failed to parse record */
			assert(cdb_sc_normal == status);
			return status;
		}
		READ_BLK_ID(long_blk_id, &blk_pter, SIZEOF(rec_hdr) + recBase + key_len);
		assert((cs_addrs->ti->total_blks > blk_pter) && (0 < blk_pter));
		new_blk_sz += (SIZEOF_BLK_ID(BLKID_64) - SIZEOF_BLK_ID(BLKID_32));
		recBase += rec_sz;								/* ready for the next record */
	}
	recBase = blkBase + SIZEOF(blk_hdr);
	assert(cs_addrs->now_crit && update_trans);
#ifdef notnow
	t_begin_crit(ERR_MUNOUPGRD);
	if (cdb_sc_normal != (status = find_dt_entry_for_blk(&dirHist, blkBase, recBase, &gvname, gvnh_reg)))
		return status;										/* WARNING assign above */
#endif
	dirHist = gv_target->hist.h[level];
	split_blks_added = split_levels_added = 0;
	if (0 < (space_need = new_blk_sz - blk_size))							/* WARNING assignment */
	{	/* insufficient room */
		space_need += SIZEOF(blk_hdr) + ((rec_hdr_ptr_t)recBase)->rsiz + (blk_size >> 3);
		assert((blk_size >> 1) > space_need);						/* paranoid check */
		DBGUPGRADE(util_out_print("splitting level !UL directory block @x!@XQ", TRUE, level, &curr_blk));
		mu_reorg_process = TRUE;
		if (cdb_sc_normal != (status = mu_split(level, space_need, space_need, &split_blks_added, &split_levels_added)))
			return status;							/* split failed; WARNING: assign above */
		mu_reorg_process = FALSE;
		if ((trans_num)0 == t_end(&(gv_target->hist), NULL, TN_NOT_SPECIFIED))			/* WARNING assignment */
=======
			continue;
		}
		if (was_asyncio_enabled = csa->hdr->asyncio)	/* WARNING assignment */
>>>>>>> f9ca5ad6 (GT.M V7.1-000)
		{
			fn_len = (size_t)rptr->reg->dyn.addr->fname_len + 1; /* Includes null terminator */
			assert(sizeof(fn) >= fn_len);
			memcpy(fn, (char *)rptr->reg->dyn.addr->fname, fn_len);
		}
<<<<<<< HEAD
		t_abort(gv_cur_region, cs_addrs);						/* do crit and other cleanup */
		new_blk_sz = ((blk_hdr_ptr_t)blkBase)->bsiz;
		assert((new_blk_sz <= blk_size) && split_blks_added);
		tot_splt_cnt += split_blks_added;
		tot_levl_cnt += split_levels_added;
		assert(!update_trans && !need_kip_incr);
		t_begin_crit(ERR_MUNOUPGRD);
#ifdef notnow
		if (cdb_sc_normal != (status = find_dt_entry_for_blk(&dirHist, blkBase, recBase, &gvname, gvnh_reg)))
		{											/* WARNING assign above */
			assert(cdb_sc_normal == status);
			return status;
		}
		dirHist = gv_target->hist.h[level];
#endif
	}
	assert(new_blk_sz < blk_size);
	/* Finally upgrade the block */
	dirHist.blk_num = curr_blk;
	if (NULL == (dirHist.buffaddr = t_qread(dirHist.blk_num, (sm_int_ptr_t)&dirHist.cycle, &dirHist.cr)))	/* WARNING assign*/
	{	/* Failed to read the indicated block */
		status = (enum cdb_sc)rdfail_detail;
		assert(cdb_sc_normal == status);
		return status;
	}
	assert(GDSV7m != blk_ver);
	dirHist.cr->ondsk_blkver = GDSV7m;								/* maintain as needed */
	assert((dirHist.buffaddr == blkBase) && (dirHist.level == level));
	CHECK_AND_RESET_UPDATE_ARRAY;
	BLK_INIT(bs_ptr, bs1);
	BLK_ADDR(v7bp, new_blk_sz, unsigned char);
	v7recBase = v7bp + SIZEOF(blk_hdr);
	((blk_hdr_ptr_t)v7bp)->bsiz = new_blk_sz;
	recBase = blkBase + SIZEOF(blk_hdr);
	for (rec_sz = 0; recBase < blkEnd; recBase += rec_sz, v7recBase += v7_rec_sz)
	{	/* Update the recBase and v7recBase pointers to point to the next record */
		/* Because blocks have pointers rather than application data, no spanning & bsiz not a worry */
		/* TODO: simplify as no collation info to deal with */
		status = read_record(&rec_sz, &key_cmpc, &key_len, key_buff, level, &dirHist, recBase);
		if ((cdb_sc_normal != status) && (cdb_sc_starrecord != status))
		{
			assert(cdb_sc_normal == status);
			return status;
		}
		GET_BLK_ID_32(blk_pter, (recBase + SIZEOF(rec_hdr) + key_len));
		assert(blk_pter);
		v7_rec_sz = rec_sz + (SIZEOF_BLK_ID(BLKID_64) - SIZEOF_BLK_ID(BLKID_32));
		assert(blk_size > v7_rec_sz);
		/* Push the revised record into the update array */
		memcpy(v7recBase, recBase, SIZEOF(rec_hdr) + key_len);
		assert((unsigned short)v7_rec_sz == v7_rec_sz);
		((rec_hdr_ptr_t)v7recBase)->rsiz = (unsigned short)v7_rec_sz;
		PUT_BLK_ID_64((v7recBase + SIZEOF(rec_hdr) + key_len), blk_pter);
		assert(rec_sz = (SIZEOF(rec_hdr) + key_len + SIZEOF_BLK_ID(BLKID_32)));
	}
	BLK_SEG(bs_ptr, v7bp + SIZEOF(blk_hdr), new_blk_sz - SIZEOF(blk_hdr));
	assert(blk_seg_cnt == new_blk_sz);
	if (!BLK_FINI(bs_ptr, bs1))
	{
		status = cdb_sc_blkmod;								/* failed to finalize the update */
		assert(cdb_sc_normal == status);
		return status;
	}
	t_write(&dirHist, (unsigned char *)bs1, 0, 0, level, TRUE, TRUE, GDS_WRITE_KILLTN);
	inctn_opcode = inctn_mu_reorg;
	if ((trans_num)0 == t_end(&(gv_target->hist), NULL, TN_NOT_SPECIFIED))
	{
		status = t_fail_hist[t_tries - 1];
		assert(cdb_sc_normal == status);
		return status;
	}
	if (cdb_sc_normal != (status = gvcst_lftsib(&left_hist)))	/* TODO How to deal with split; WARNING assignment */
	{
		if (cdb_sc_endtree == status)
			status = cdb_sc_normal;
	}
	if (cdb_sc_normal != status)
	{
		assert(cdb_sc_normal == status);
		return status;
	}
	cs_addrs->hold_onto_crit = FALSE;
	t_abort(gv_cur_region, cs_addrs);							/* do crit and other cleanup */
	cs_data->blks_to_upgrd--;
	DBGUPGRADE(util_out_print("adjusted level !UL index block @x!@XQ", TRUE, level, &curr_blk));
	return cdb_sc_normal; 									/* finished upgrading this block */
}

#ifdef notnow
enum cdb_sc find_big_sib(block_id blk, int level)
{	/* given a block, and level find next sibling to the right, in contrast to gvcst_rtsib, which only works with level 0 */
	/* read record size, get next offset, check for end of block, read next record, get record size, in order to get pointer,
	 * read block, read first record, check for *-key, get key, search on key to leave gv_target set up
	 * if end of block go up a level, check for too high and resume
	 * if *-key go down levels in search of a key, check for level 0, and resume
	 * if level 0 or too hign then cdb_sc_endtree
	 */
	block_id		blk_ptr;
	int			i, key_cmpc, key_len, rec_sz;
	int4			lev, status;
	gvnh_reg_t		*gvnh_reg;
	mname_entry		*gvname;
	sm_uc_ptr_t		recBase;
	srch_blk_status		dirHist;
	unsigned char		key_buff[MAX_KEY_SZ + 1];

	if (gv_target->hist.depth == level)
		return cdb_sc_endtree;
	lev = level;
	dirHist.blk_num = gv_target->hist.h[--lev].blk_num;
	do
	{
		if (NULL == (dirHist.buffaddr = t_qread(dirHist.blk_num, (sm_int_ptr_t)&dirHist.cycle, &dirHist.cr)))
		{
			t_retry(rdfail_detail);
			continue;
		}
		if (dirHist.buffaddr != gv_target->hist.h[lev].buffaddr)
		{
			t_retry(cdb_sc_losthist);
			continue;
		}
		recBase = dirHist.buffaddr + gv_target->hist.h[lev].curr_rec.offset;	/* TODO: is offset in gv_target reliable? */
		recBase += ((rec_hdr_ptr_t)recBase)->rsiz;
		if ((dirHist.buffaddr + ((blk_hdr_ptr_t)dirHist.buffaddr)->bsiz) < recBase)
		{
			if (0 == lev)
				return cdb_sc_endtree;
			dirHist.blk_num = gv_target->hist.h[--lev].blk_num;
			continue;
		}
		recBase = dirHist.buffaddr + SIZEOF(blk_hdr);
		recBase += ((rec_hdr_ptr_t)recBase)->rsiz;
		dirHist.blk_num = *(block_id *)(recBase - SIZEOF_BLK_ID(IS_64_BLK_ID(dirHist.buffaddr)));
		if (NULL == (dirHist.buffaddr = (t_qread(dirHist.blk_num,(sm_int_ptr_t)&dirHist.cycle, &dirHist.cr))))
		{
			t_retry(rdfail_detail);
			continue;
		}
		dirHist.blk_num = dirHist.blk_num;
		dirHist.level = lev;
		recBase = dirHist.buffaddr + SIZEOF(blk_hdr);
		if (cdb_sc_starrecord == (status = read_record(&rec_sz, &key_cmpc, &key_len, key_buff, lev, &dirHist, recBase)))
		{
			dirHist.blk_num = *(block_id *)(recBase + rec_sz - SIZEOF_BLK_ID(IS_64_BLK_ID(dirHist.buffaddr)));
			continue;
		}
		if (cdb_sc_normal != status)
		{
			t_retry(status);
			continue;
		}
		/* block should have a potentially useful key in the first record */
		assert(MAX_KEY_SZ >= key_len);
		gvname->var_name.len = MIN(key_len, MAX_MIDENT_LEN);
		gvname->var_name.addr = (char *)key_buff;
		memcpy(gv_currkey->base, key_buff, key_len + 1);				/* the +1 gets a key_delimiter */
		gv_currkey->end = key_len - 1;
		COMPUTE_HASH_MNAME(gvname);
		GV_BIND_NAME_ONLY(gd_header, gvname, gvnh_reg);
		assert(KEY_DELIMITER == gv_currkey->base[gv_currkey->end]);
		gv_altkey = gv_currkey;
		gv_target->clue.end = 0;
		//		for (i = 0; i < MAX_BT_DEPTH; i++)
		//			gv_target->hist.h[i].level = i;
		if (cdb_sc_normal != (status = gvcst_search(gv_currkey, NULL)))
		{
			t_retry(status);
			continue;
		}
		if ((trans_num)0 == t_end(&gv_target->hist, NULL, TN_NOT_SPECIFIED))
		{
			t_retry(t_fail_hist[t_tries]);
			continue;
		}
		return status;
	} while (TRUE);
#ifdef notnow
		/* Region can be upgraded, so run it down and get standalone TODO: ? abuse rollback flags to avoid standalone ? */
=======
		/* Obtain standalone access */
>>>>>>> f9ca5ad6 (GT.M V7.1-000)
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
<<<<<<< HEAD
		gvcst_init(reg);
#endif
		csa = cs_addrs;
		csd = cs_data;
		blk_size = csd->blk_size;
		util_out_print("!/Region !AD : MUPIP UPGRADE started", TRUE, REG_LEN_STR(reg));
		/* Make room for the enlarged master bitmap */
		/* TODO: any file header calcs/updates make journal file switch or perhaps later ? */
		/* calculate speace needed to larger (64-bit) pointers */
		csd->max_rec = (blk_size - SIZEOF(blk_hdr))	/* maximum records using the smallest possible (1 char) keys */
			/ (SIZEOF(rec_hdr) + (3 * SIZEOF(KEY_DELIMITER)) + SIZEOF_BLK_ID(BLKID_64)); /* 2 delimiters + 1 char key */
		csd->i_reserved_bytes = csd->max_rec * (SIZEOF_BLK_ID(BLKID_64) - SIZEOF_BLK_ID(BLKID_32));
		blocks_needed = (csd->trans_hist.total_blks - csd->trans_hist.free_blocks) >> 3; /* pessimistic cnt of index blks */
		blocks_needed = DIVIDE_ROUND_UP(blocks_needed * csd->i_reserved_bytes, blk_size);
		status = mu_upgrade_bmm(reg, blocks_needed);
		if (SS_NORMAL != status)
		{	/* There was an error while enlarging the master bitmap so cancel upgrade */
			/* Release standalone access */
#ifdef notnow	/* TODO: why are these suppressed? */
			assert(FILE_INFO(gv_cur_region)->grabbed_access_sem);
			db_ipcs_reset(gv_cur_region);
#endif
			/* Print status message and move onto next region */
			util_out_print("Region !AD : Error while attempting to make room for enlarged master bitmap.",
					FALSE, REG_LEN_STR(reg));
			util_out_print("  Moving onto next region.",TRUE);
			error = TRUE;
			continue;
		}
#ifdef notnow
		/* Now that DB has been upgraded to V7 rundown and reopen the DB inorder to flush the block cache */
		mu_rndwn_file(gv_cur_region, FALSE);
		assert(FILE_INFO(gv_cur_region)->grabbed_access_sem);
		db_ipcs_reset(gv_cur_region);
		if (EXIT_NRM != gds_rundown(CLEANUP_UDI_FALSE))
		{	/* Failed to rundown the DB */
			error = TRUE;
			util_out_print("Region !AD : Failed to rundown the database.",
					TRUE, REG_LEN_STR(reg));
			continue;
		}
		gvcst_init(reg);
=======
		gvcst_init(reg, NULL);
>>>>>>> f9ca5ad6 (GT.M V7.1-000)
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
		util_out_print("Region !AD : MUPIP MASTERMAP UPGRADE started (!AD)",
				TRUE, REG_LEN_STR(reg), DB_LEN_STR(reg));
		blocks_needed = csd->trans_hist.total_blks - csd->trans_hist.free_blocks;
		blocks_needed -= (csd->trans_hist.total_blks / BLKS_PER_LMAP);	/* Subtract LMAPs from the count */
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
			udi = FILE_INFO(reg);	/* Allocate an aligned buffer for new bmm (in case AIO), see DIO_BUFF_EXPAND_IF_NEEDED */
			new_bmm_size = ROUND_UP(MASTER_MAP_SIZE_DFLT, cs_data->blk_size) + OS_PAGE_SIZE;
			bml_buff = malloc(new_bmm_size);
			bmm_base = (sm_uc_ptr_t)ROUND_UP2((sm_long_t)bml_buff, OS_PAGE_SIZE);
			assert(OS_PAGE_SIZE >= (bmm_base - bml_buff));
			memset(bmm_base, BMP_EIGHT_BLKS_FREE, MASTER_MAP_SIZE_DFLT); 		/* Initialize entire bmm to FREE */
			DB_LSEEKWRITE(csa, udi, udi->fn, udi->fd, SGMNT_HDR_LEN, bmm_base,	/* Write new BMM after file header */
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
<<<<<<< HEAD
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_MUPGRDSUCC, 6, DB_LEN_STR(reg), RTS_ERROR_LITERAL("upgraded"),
				ydb_release_name_len, ydb_release_name);
=======
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
		if (was_asyncio_enabled)
		{
			util_out_print("Region !AD : Restoring ASYNCIO setting fo !AD", TRUE, REG_LEN_STR(reg), DB_LEN_STR(reg));
			csa->hdr->asyncio = TRUE;
		}
		RELEASE_ACCESS_SEMAPHORE_AND_RUNDOWN(gv_cur_region, error);
		util_out_print("Region !AD : MUPIP MASTERMAP UPGRADE of !AD completed", TRUE, REG_LEN_STR(reg), DB_LEN_STR(reg));
>>>>>>> f9ca5ad6 (GT.M V7.1-000)
	}
	if (file)
		mu_gv_cur_reg_free();
	mupip_exit(error ? ERR_MUNOFINISH : SS_NORMAL);
	return;
}
