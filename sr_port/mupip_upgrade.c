/****************************************************************
 *								*
 * Copyright (c) 2005-2021 Fidelity National Information	*
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
#include "gdsbt.h"
#include "gdsfhead.h"
#include "v6_gdsfhead.h"
#include "gdsblkops.h"
#include "gdscc.h"
#include "gdsbml.h"
#include "gtm_string.h"
#include "gtm_common_defs.h"
#include "util.h"
#include "filestruct.h"
#include "cli.h"
#include "mu_reorg.h"
#include "gvcst_protos.h"
#include "muextr.h"
#include "memcoherency.h"
#include "sleep_cnt.h"
#include "interlock.h"
#include "hashtab_mname.h"
#include "wcs_flu.h"
#include "jnl.h"	/* For fd_type */

/* Prototypes */
#include "mu_getlst.h"
#include "mu_rndwn_file.h"
#include "gdskill.h"
#include "mu_upgrade_bmm.h"
#include "mupip_exit.h"
#include "t_qread.h"
#include "targ_alloc.h"
#include "change_reg.h"
#include "t_abort.h"
#include "t_begin_crit.h"
#include "t_create.h"
#include "t_write.h"
#include "t_write_map.h"
#include "t_end.h"
#include "t_retry.h"
#include "db_header_conversion.h"
#include "anticipatory_freeze.h"
#include "gdsfilext.h"
#include "bmm_find_free.h"
#include "mupip_reorg.h"
#include "gvcst_bmp_mark_free.h"
#include "mu_gv_cur_reg_init.h"
#include "db_ipcs_reset.h"
#include "mu_upgrd_dngrd_confirmed.h"
#include "wcs_recover.h"
#include "gvt_inline.h"		/* Before gtmio.h, which includes the open->open64 macro on AIX, which we don't want here. */
#include "gtmio.h"
#include "clear_cache_array.h"
#include "bit_clear.h"
#include "bit_set.h"
#include "gds_blk_upgrade.h"
#include "spec_type.h"
#include "gtm_stat.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "eintr_wrappers.h"
#include "gtm_stdlib.h"

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
#include "gdsblk.h"
#include "gds_rundown.h"
#include "error.h"
#include "gtmmsg.h"
#include "repl_sp.h"
#include "mupip_upgrade.h"
#include "mu_upgrd_dngrd_hdr.h"
#include "mu_outofband_setup.h"
#include "mu_all_version_standalone.h"
#include "db_write_eof_block.h"
#include "dpgbldir.h"
#include "op.h"

GBLREF	bool			error_mupip;
GBLREF	boolean_t		mu_reorg_process, mu_reorg_upgrd_dwngrd_in_prog, need_kip_incr;
GBLREF	char			*update_array, *update_array_ptr;	/* for the BLK_* macros */
GBLREF	gd_addr			*gd_header;
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_key			*gv_altkey, *gv_currkey;
GBLREF	gv_namehead		*gv_target, *gv_target_list, *reorg_gv_target;
GBLREF	inctn_opcode_t		inctn_opcode;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	tp_region		*grlist;
GBLREF	unsigned char		rdfail_detail, t_fail_hist[];
GBLREF	unsigned int		t_tries;
GBLREF	uint4			update_trans;
GBLREF	uint4			update_array_size;			/* for the BLK_* macros */

GBLREF	boolean_t		exit_handler_active;
GBLREF	boolean_t		exit_handler_complete;

<<<<<<< HEAD
LITREF  char            	ydb_release_name[];
LITREF  int4           		ydb_release_name_len;

GBLDEF	sem_info		*sem_inf;

STATICFNDCL void mupip_upgrade_cleanup(void);

=======
>>>>>>> 52a92dfd (GT.M V7.0-001)
error_def(ERR_BADDBVER);
error_def(ERR_DBFILERR);
error_def(ERR_DBFILOPERR);
error_def(ERR_DBMAXREC2BIG);
error_def(ERR_DBMINRESBYTES);
error_def(ERR_DBNOTGDS);
error_def(ERR_DBOPNERR);
error_def(ERR_DBPREMATEOF);
error_def(ERR_DBRDONLY);
error_def(ERR_GVGETFAIL);
error_def(ERR_GTMCURUNSUPP);
error_def(ERR_MUINFOUINT4);
error_def(ERR_MUINFOUINT8);
error_def(ERR_MUNOFINISH);
error_def(ERR_MUNODBNAME);
error_def(ERR_MUNOUPGRD);
error_def(ERR_MUPGRDSUCC);
error_def(ERR_MUSTANDALONE);
error_def(ERR_MUUPGRDNRDY);
error_def(ERR_PREMATEOF);
error_def(ERR_STATSDBNOTSUPP);
error_def(ERR_SYSCALL);
error_def(ERR_TEXT);

#define BIG_GVNAME		"zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz"
#define SML_GVNAME		"%"

static sem_info	*sem_inf;
static void mupip_upgrade_cleanup(void);
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
	mval		first_key = DEFINE_MVAL_STRING(MV_STR | MV_NUM_APPROX,  0, 0, 0, (char *)first_buff, 0, 0);
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
<<<<<<< HEAD
	/* Initialization */
	DEFINE_EXIT_HANDLER(mupip_upgrade_cleanup, TRUE);
	/* Structure checks .. */
	assert((24 * 1024) == SIZEOF(v15_sgmnt_data));	/* Verify V4 file header hasn't suddenly increased for some odd reason */
	sem_inf = (sem_info *)malloc(SIZEOF(sem_info) * FTOK_ID_CNT);
	memset(sem_inf, 0, SIZEOF(sem_info) * FTOK_ID_CNT);
	db_fn_len = SIZEOF(db_fn);
	if (!cli_get_str("FILE", db_fn, &db_fn_len))
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_MUNODBNAME);
	db_fn_len = MIN(db_fn_len, MAX_FN_LEN);
	db_fn[db_fn_len] = '\0';	/* Null terminate */
	/* Need to find out if this is a statsDB file. This necessitates opening the file to read the sgmnt_data
	 * file header before we have the proper locks obtained for it so after checking, the file is closed again
	 * so it can be opened under lock to prevent race conditions.
	 */
	if (FD_INVALID != (channel = OPEN(db_fn, O_RDONLY)))
=======
	/* Structure checks */
	mupip_exit(ERR_GTMCURUNSUPP);
	assert((8192) == SIZEOF(sgmnt_data));		/* Verify V7 file header hasn't changed */
	assert((8192) == SIZEOF(v6_sgmnt_data));	/* Verify V6 file header hasn't changed */
	/* Get list of regions to upgrade */
	gvinit();	/* initialize gd_header (needed by the later call to mu_getlst) */
	/* TODO: support file or region */
	mu_getlst("REGION", SIZEOF(tp_region)); /* get the parm for the REGION qualifier */
	if (error_mupip)
>>>>>>> 52a92dfd (GT.M V7.0-001)
	{
		util_out_print("!/MUPIP UPGRADE cannot proceed with above errors!/", TRUE);
		mupip_exit(ERR_MUNOACTION);	/* TODO: here & elsewhere, why the change from ERR_MUNOUPGRD? */
	}
	if (!mu_upgrd_dngrd_confirmed())
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TEXT, 2, LEN_AND_LIT("Upgrade canceled by user"));
		mupip_exit(ERR_MUNOACTION);
	}
<<<<<<< HEAD
	gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TEXT, 2, LEN_AND_LIT("Mupip upgrade started"));
	UNIX_ONLY(mu_all_version_get_standalone(db_fn, sem_inf));
	mu_outofband_setup();	/* Will ignore user interrupts. Note that now the
				 * elapsed time for this is order of milliseconds */
	/* ??? Should we set this just before DB_DO_FILE_WRITE to have smallest time window of ignoring signal? */
	if (FD_INVALID == (channel = OPEN(db_fn, O_RDWR)))	/* Note assignment */
	{
		save_errno = errno;
		if (FD_INVALID != (channel = OPEN(db_fn, O_RDONLY)))
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(10) ERR_DBRDONLY, 2, db_fn_len, db_fn, errno, 0,
				   MAKE_MSG_TYPE(ERR_TEXT, ERROR), 2, LEN_AND_LIT("Cannot upgrade read-only database"));
		else
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_DBOPNERR, 2, db_fn_len, db_fn, save_errno);
		mupip_exit(ERR_MUNOUPGRD);
	}
	/* get file status */
	FSTAT_FILE(channel, &stat_buf, fstat_res);
	if (-1 == fstat_res)
	{
		F_CLOSE(channel, rc); /* resets "channel" to FD_INVALID */
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("fstat"), CALLFROM, errno);
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_DBOPNERR, 2, db_fn_len, db_fn);
		mupip_exit(ERR_MUNOUPGRD);
	}
#	if defined(__MVS__)
	if (-1 == gtm_zos_tag_to_policy(channel, TAG_BINARY, &realfiletag))
		TAG_POLICY_GTM_PUTMSG(db_fn, errno, realfiletag, TAG_BINARY);
#	endif
	v15_file_size = stat_buf.st_size;
	v15_csd_size = SIZEOF(v15_sgmnt_data);
	DO_FILE_READ(channel, 0, &v15_csd, v15_csd_size, status, status2);
	if (SS_NORMAL != status)
	{
		F_CLOSE(channel, rc); /* resets "channel" to FD_INVALID */
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_DBFILOPERR, 2, db_fn_len, db_fn, status);
		mupip_exit(ERR_MUNOUPGRD);
	}
	if (!memcmp(v15_csd.label, GDS_LABEL, STR_LIT_LEN(GDS_LABEL)))
	{	/* Check if the V5 database is old(supports only 128M blocks) if so update the V5 database to support
		 * to 224M blocks.
=======
	/* Iterate over regions */
	for (rptr = grlist;  NULL != rptr;  rptr = rptr->fPtr)
	{	/* Initialize the region to check if eligible for upgrade */
		/* TODO: implement interlock so BACKUP, INTEG (snapshot), REORG ROLLBACK don't run concurrently with UPGRADE
>>>>>>> 52a92dfd (GT.M V7.0-001)
		 */
		reg = rptr->reg;
		gv_cur_region = reg;
		gvcst_init(reg, NULL);
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
<<<<<<< HEAD
		if (MASTER_MAP_SIZE_V5_OLD == csd.master_map_len)
		{
			/* We have detected the master map which supports only 128M blocks so we need to
			 * bump it up to one that supports 224M blocks. */
			csd.master_map_len = MASTER_MAP_SIZE_V5;
			assert(START_VBN_V5 == csd.start_vbn);
			DEBUG_ONLY (
				norm_vbn = DIVIDE_ROUND_UP(SIZEOF_FILE_HDR_V5, DISK_BLOCK_SIZE) + 1;
				assert(START_VBN_V5 == norm_vbn);
			)
			csd.free_space = 0;
			DB_DO_FILE_WRITE(channel, 0, &csd, SIZEOF(csd), status, status2);
			if (SS_NORMAL != status)
			{
				F_CLOSE(channel, rc); /* resets "channel" to FD_INVALID */
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_DBFILOPERR, 2, db_fn_len, db_fn, status);
				mupip_exit(ERR_MUNOUPGRD);
			}
			memset(new_v5_master_map, BMP_EIGHT_BLKS_FREE, (MASTER_MAP_SIZE_V5 - MASTER_MAP_SIZE_V5_OLD));
			DB_DO_FILE_WRITE(channel, SIZEOF(csd) + MASTER_MAP_SIZE_V5_OLD, new_v5_master_map,
							(MASTER_MAP_SIZE_V5 - MASTER_MAP_SIZE_V5_OLD), status, status2);
			if (SS_NORMAL != status)
			{
				F_CLOSE(channel, rc); /* resets "channel" to FD_INVALID */
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_DBFILOPERR, 2, db_fn_len, db_fn, status);
				mupip_exit(ERR_MUNOUPGRD);
			}
			F_CLOSE(channel, rc);	/* resets "channel" to FD_INVALID */
			UNIX_ONLY(mu_all_version_release_standalone(sem_inf));
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TEXT, 2,
					LEN_AND_LIT("Maximum master map size is now increased from 32K to 56K"));
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_MUPGRDSUCC, 6, db_fn_len, db_fn,
					RTS_ERROR_LITERAL("upgraded"), ydb_release_name_len, ydb_release_name);
			mupip_exit(SS_NORMAL);
=======
		if (RDBF_STATSDB_MASK == csd->reservedDBFlags)
		{	/* TODO: statsDB need upgrades */
			error = TRUE;
			gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_STATSDBNOTSUPP, 2, DB_LEN_STR(reg));
			continue; /* move onto next region */
>>>>>>> 52a92dfd (GT.M V7.0-001)
		}
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
			gvcst_init(reg, NULL);
			change_reg();
			csa = cs_addrs;
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
			{	/* Failed to rundown the DB */
				error = TRUE;
				util_out_print("Region !AD : Failed to rundown the database.",
					       TRUE, REG_LEN_STR(reg));
				continue;
			}
			gvcst_init(reg, NULL);
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
				       gtm_release_name_len, gtm_release_name);
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
					if (cdb_sc_normal != (status = read_record(&rec_sz, &key_cmpc, &key_len, key_buff,
							level, &dirHist, recBase)))
					{	/* failed to parse record */
						l_tries++;
						continue;
					}
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
					if (cdb_sc_normal != (status = upgrade_gvt(blk_ptr, blk_size, reg, &root_name)))
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
	enum db_ver	blk_ver;
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
<<<<<<< HEAD
		F_CLOSE(channel, rc);	/* resets "channel" to FD_INVALID */
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) MAKE_MSG_TYPE(ERR_TEXT, ERROR), 2,
							LEN_AND_LIT("Database creation in progress"));
		mupip_exit(ERR_MUNOUPGRD);
	}
	if (v15_csd.freeze)
	{
		F_CLOSE(channel, rc);	/* resets "channel" to FD_INVALID */
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) MAKE_MSG_TYPE(ERR_TEXT, ERROR), 2, LEN_AND_LIT("Database is frozen"));
		mupip_exit(ERR_MUNOUPGRD);
	}
	if (v15_csd.wc_blocked)
	{
		F_CLOSE(channel, rc);	/* resets "channel" to FD_INVALID */
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) MAKE_MSG_TYPE(ERR_TEXT, ERROR),
			   2, LEN_AND_LIT("Database modifications are disallowed because wc_blocked is set"));
		mupip_exit(ERR_MUNOUPGRD);
	}
	if (v15_csd.file_corrupt)
	{
		F_CLOSE(channel, rc);	/* resets "channel" to FD_INVALID */
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) MAKE_MSG_TYPE(ERR_TEXT, ERROR), 2, LEN_AND_LIT("Database corrupt"));
		mupip_exit(ERR_MUNOUPGRD);
	}
	if (v15_csd.intrpt_recov_tp_resolve_time || v15_csd.intrpt_recov_resync_seqno || v15_csd.recov_interrupted
	    || v15_csd.intrpt_recov_jnl_state || v15_csd.intrpt_recov_repl_state)
	{
		F_CLOSE(channel, rc);	/* resets "channel" to FD_INVALID */
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) MAKE_MSG_TYPE(ERR_TEXT, ERROR), 2,
								LEN_AND_LIT("Recovery was interrupted"));
		mupip_exit(ERR_MUNOUPGRD);
	}
	if (GDSVCURR != v15_csd.certified_for_upgrade_to)
	{
		F_CLOSE(channel, rc);	/* resets "channel" to FD_INVALID */
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUUPGRDNRDY, 4, db_fn_len, db_fn,
								ydb_release_name_len, ydb_release_name);
		mupip_exit(ERR_MUNOUPGRD);
	}
	max_max_rec_size = v15_csd.blk_size - SIZEOF(blk_hdr);
	if (VMS_ONLY(9) UNIX_ONLY(8) > v15_csd.reserved_bytes)
	{
		F_CLOSE(channel, rc); /* resets "channel" to FD_INVALID */
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_DBMINRESBYTES, 4, VMS_ONLY(9) UNIX_ONLY(8), v15_csd.reserved_bytes);
		mupip_exit(ERR_MUNOUPGRD);
	}
	if (v15_csd.max_rec_size > max_max_rec_size)
	{
		F_CLOSE(channel, rc); /* resets "channel" to FD_INVALID */
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(5) ERR_DBMAXREC2BIG, 3,
			v15_csd.max_rec_size, v15_csd.blk_size, max_max_rec_size);
		mupip_exit(ERR_MUNOUPGRD);
	}
	gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUINFOUINT4, 4, LEN_AND_LIT("Old file header size"),
											v15_csd_size, v15_csd_size);
	gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUINFOUINT8, 4, LEN_AND_LIT("Old file length"),
											&v15_file_size, &v15_file_size);
	gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUINFOUINT4, 4, LEN_AND_LIT("Old file start_vbn"),
											v15_csd.start_vbn, v15_csd.start_vbn);
	gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUINFOUINT4, 4, LEN_AND_LIT("Old file gds blk_size"),
											v15_csd.blk_size, v15_csd.blk_size);
	gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUINFOUINT4, 4, LEN_AND_LIT("Old file total_blks"),
		   v15_csd.trans_hist.total_blks, v15_csd.trans_hist.total_blks);
	assert(ROUND_DOWN2(v15_csd.blk_size, DISK_BLOCK_SIZE) == v15_csd.blk_size);
	assert(((off_t)v15_csd.start_vbn) * DISK_BLOCK_SIZE +
			(off_t)v15_csd.trans_hist.total_blks * v15_csd.blk_size == v15_file_size);
	/* Now call mu_upgrd_header() to do file header upgrade in memory */
        mu_upgrd_header(&v15_csd, &csd);
	csd.master_map_len = MASTER_MAP_SIZE_V4;	/* V5 master map is not part of file header */
	memcpy(new_master_map, v15_csd.master_map, MASTER_MAP_SIZE_V4);
	DB_DO_FILE_WRITE(channel, 0, &csd, SIZEOF(csd), status, status2);
	if (SS_NORMAL != status)
	{
		F_CLOSE(channel, rc); /* resets "channel" to FD_INVALID */
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_DBFILOPERR, 2, db_fn_len, db_fn, status);
		mupip_exit(ERR_MUNOUPGRD);
	}
	DB_DO_FILE_WRITE(channel, SIZEOF(csd), new_master_map, MASTER_MAP_SIZE_V4, status, status2);
	if (SS_NORMAL != status)
	{
		F_CLOSE(channel, rc); /* resets "channel" to FD_INVALID */
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_DBFILOPERR, 2, db_fn_len, db_fn, status);
		mupip_exit(ERR_MUNOUPGRD);
	}
	/* The V4 database file would have a 512-byte EOF block. Enlarge it to be a GDS-block instead (V6 format). */
	db_write_eof_block(NULL, channel, v15_csd.blk_size, v15_file_size - DISK_BLOCK_SIZE, &(TREF(dio_buff)));
	F_CLOSE(channel, rc);	/* resets "channel" to FD_INVALID */
	mu_all_version_release_standalone(sem_inf);
	gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_MUPGRDSUCC, 6, db_fn_len, db_fn, RTS_ERROR_LITERAL("upgraded"),
		   							ydb_release_name_len, ydb_release_name);
	mupip_exit(SS_NORMAL);
}

STATICFNDEF void mupip_upgrade_cleanup(void)
{
	if (exit_handler_active)
		return;
	exit_handler_active = TRUE;
	if (sem_inf)
		mu_all_version_release_standalone(sem_inf);
	exit_handler_complete = TRUE;
=======
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
			if (BSTAR_REC_SIZE == (rec_sz = ((rec_hdr_ptr_t)recBase)->rsiz))		/* WARNING assignment*/
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
			continue;
	} while (level);
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
		{
			status = t_fail_hist[t_tries - 1];
			assert(cdb_sc_normal == status);
			return status;
		}
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
	for (rec_sz = v7_rec_sz = 0; recBase < blkEnd; recBase += rec_sz, v7recBase += v7_rec_sz)
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
	if (cdb_sc_normal == (status = gvcst_lftsib(&left_hist)))	/* TODO How to deal with split; WARNING assignment */
		left_blk_status = left_hist.h;
	else if (cdb_sc_endtree == status)
		status = cdb_sc_normal;
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
>>>>>>> 52a92dfd (GT.M V7.0-001)
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
		gvcst_init(reg, NULL);
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
		gvcst_init(reg, NULL);
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
				gtm_release_name_len, gtm_release_name);
	}
	if (error)
		mupip_exit(ERR_MUNOFINISH);
	else
		mupip_exit(SS_NORMAL);
}
#endif
