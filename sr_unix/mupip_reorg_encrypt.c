/****************************************************************
 *								*
 * Copyright (c) 2015-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

#include "gdsroot.h"
#include "gdsbt.h"
#include "gdskill.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "hashtab_int4.h"
#include "gdscc.h"
#include "buddy_list.h"
#include "tp.h"
#include "gdsblk.h"
#include "gdsblkops.h"
#include "cli.h"
#include "mu_getlst.h"
#include "mupip_exit.h"
#include "util.h"
#include "targ_alloc.h"
#include "wcs_flu.h"
#include "t_qread.h"
#include "gdsbml.h"
#include "cert_blk.h"
#include "t_begin.h"
#include "t_retry.h"
#include "t_write_map.h"
#include "t_write.h"
#include "t_end.h"
#include "sleep_cnt.h"
#include "gtm_c_stack_trace.h"
#include "is_proc_alive.h"
#include "gtmsecshr.h"
#include "wcs_backoff.h"
#include "wcs_sleep.h"
#include "gtm_sem.h"
#include "gtm_semutils.h"
#include "eintr_wrapper_semop.h"
#include "do_semop.h"
#include "ftok_sems.h"
#include "have_crit.h"
#include "mupip_reorg_encrypt.h"
#include "t_abort.h"
#include "interlock.h"
#include "repl_msg.h"			/* for gtmsource.h */
#include "gtmsource.h"			/* for jnlpool_addrs_ptr_t */

GBLREF bool		error_mupip;
GBLREF bool		mu_ctrlc_occurred;
GBLREF bool		mu_ctrly_occurred;
GBLREF char		*update_array;			/* for the BLK_INIT/BLK_SEG/BLK_ADDR macros */
GBLREF char		*update_array_ptr;		/* for the BLK_INIT/BLK_SEG/BLK_ADDR macros */
GBLREF cw_set_element	cw_set[];			/* create write set */
GBLREF boolean_t	mu_reorg_process;
GBLREF gd_region	*ftok_sem_reg;
GBLREF gd_region	*gv_cur_region;
GBLREF gv_namehead	*gv_target;
GBLREF gv_namehead	*gv_target_list;
GBLREF inctn_detail_t	inctn_detail;			/* holds detail to fill in to inctn jnl record */
GBLREF inctn_opcode_t	inctn_opcode;
GBLREF int4		gv_keysize;
GBLREF jnl_gbls_t	jgbl;
GBLREF uint4		mu_reorg_encrypt_in_prog;	/* Non-zero if MUPIP REORG ENCRYPT is in progress. The numeric value is
							 * needed to set a special temporary indication to a dsk_read call on a
							 * local buffer. */
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgmnt_data_ptr_t	cs_data;
GBLREF tp_region	*grlist;
GBLREF uint4		process_id;
GBLREF uint4		update_array_size;		/* for the BLK_INIT/BLK_SEG/BLK_ADDR macros */
GBLREF uint4		update_trans;
GBLREF unsigned char    cw_map_depth;
GBLREF unsigned char	cw_set_depth;
GBLREF unsigned char	rdfail_detail;
GBLREF sgmnt_addrs	*reorg_encrypt_restart_csa;

error_def(ERR_BUFFLUFAILED);
error_def(ERR_BUFRDTIMEOUT);
error_def(ERR_CRITSEMFAIL);
error_def(ERR_CRYPTNOKEY);
error_def(ERR_DBFILERR);
error_def(ERR_DBNOREGION);
error_def(ERR_DBRDONLY);
error_def(ERR_DSEBLKRDFAIL);
error_def(ERR_JNLEXTEND);
error_def(ERR_MUNOACTION);
error_def(ERR_MUNOFINISH);
error_def(ERR_MUREORGFAIL);
error_def(ERR_ENCRYPTCONFLT);
error_def(ERR_MUREENCRYPTEND);
error_def(ERR_MUREENCRYPTSTART);
error_def(ERR_MUREENCRYPTV4NOALLOW);
error_def(ERR_REORGCTRLY);

STATICFNDEF boolean_t	wait_for_concurrent_reads(sgmnt_addrs *csa);
STATICFNDEF void	get_ftok_semaphore(gd_region *reg, sgmnt_addrs *csa);
STATICFNDEF void	release_ftok_semaphore(gd_region *reg, sgmnt_addrs *csa, sgmnt_data_ptr_t csd);
STATICFNDEF void	switch_journal_file(sgmnt_addrs *csa, sgmnt_data_ptr_t csd);

#define EXIT_MUPIP_REORG(STATUS)											\
MBSTART {														\
	mu_reorg_encrypt_in_prog = MUPIP_REORG_IN_PROG_FALSE;								\
	mupip_exit(STATUS);												\
} MBEND

/* Note: The below should not use MBSTART/MBEND as it does a "continue" (see MBSTART macro definition comment) */
#define CONTINUE_TO_NEXT_REGION(CSA, CSD, CNL, REG, REG_STATUS, STATUS, REORG_IN_PROG, HAVE_CRIT, ERROR, OPER, ...)	\
{															\
	if (REORG_IN_PROG)												\
	{														\
		assert((CNL)->reorg_encrypt_pid == process_id);								\
		(CNL)->reorg_encrypt_pid = 0;										\
	}														\
	if (HAVE_CRIT)													\
		rel_crit(REG);												\
	release_ftok_semaphore(REG, CSA, CSD);										\
	OPER __VA_ARGS__;												\
	if (ERROR)													\
		STATUS = REG_STATUS = ERR_MUNOFINISH;									\
	continue;													\
}

#define REORG_IN_PROG_SET	TRUE
#define REORG_IN_PROG_NOT_SET	FALSE
#define HOLDING_CRIT		TRUE
#define NOT_HOLDING_CRIT	FALSE
#define IS_ERROR		TRUE
#define IS_NOT_ERROR		FALSE

/* Perform an online (re)encryption of the specified region(s) using the specified key. A lot of code in this module is modeled on
 * mu_reorg_upgrd_dwngrd.c.
 */
void mupip_reorg_encrypt(void)
{
	char			key[GTM_PATH_MAX], hash[GTMCRYPT_HASH_LEN];
	char			*db_name, *bml_lcl_buff;
	int			db_name_len, gtmcrypt_errno, status, reg_status, status1;
	int			reg_count, i, total_blks, cycle, lcnt, bml_status;
	int4			blk_seg_cnt, blk_size, mapsize;	/* needed for BLK_INIT,BLK_SEG and BLK_FINI macros */
	unsigned short		key_len;
	gd_region		*reg;
	gtmcrypt_key_t		*handles;
	boolean_t		need_journal_switch;
	uint4			is_encrypted;
	srch_hist		alt_hist;
	srch_blk_status		*blkhist, bmlhist;
	tp_region		*rptr;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	node_local_ptr_t	cnl;
	block_id		*blkid_ptr, start_blk, start_bmp, last_bmp, curbmp, curblk;
	trans_num		curr_tn, start_tn, blk_tn;
	sm_uc_ptr_t		blkBase, bml_sm_buff;	/* shared memory pointer to the bitmap global buffer */
	cache_rec_ptr_t		cr;
	blk_segment		*bs1, *bs_ptr;
	sm_uc_ptr_t		bptr, buff;
	blk_hdr			new_hdr;
	unsigned char    	save_cw_set_depth;
	uint4			lcl_update_trans, pid, bptr_size;
	jnl_private_control	*jpc;
#	ifdef DEBUG
	uint4			reencryption_count;
	uint4			initial_blk_cnt;
#	endif
	unix_db_info		*udi;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	mu_reorg_encrypt_in_prog = MUPIP_REORG_IN_PROG_TRUE;
	status = SS_NORMAL;
	/* Get the region(s) parameter. */
	gvinit();
	error_mupip = FALSE;
	mu_getlst("REG_NAME", SIZEOF(tp_region));
	if (error_mupip)
		EXIT_MUPIP_REORG(ERR_MUNOACTION);
	else if (!grlist)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_DBNOREGION);
		EXIT_MUPIP_REORG(ERR_MUNOACTION);
	}
	/* Get the key parameter (we should not have come here unless the -ENCRYPT qualifier was supplied). */
	assert(CLI_PRESENT == cli_present("ENCRYPT"));
	key_len = SIZEOF(key);
	if (!cli_get_str("ENCRYPT", key, &key_len))
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_CRYPTNOKEY);
		EXIT_MUPIP_REORG(ERR_MUNOACTION);
	}
	/* Even though we are only specifying one key to be potentially applied to multiple regions, we need to ensure that every
	 * database file has an association with the specified key file in the encryption configuration. The fact that we are
	 * allocating space for all of the encryption handles instead of one should not matter much in terms of memory because each
	 * handles is quite small and we do not expect multiple REORG -ENCRYPT processes running concurrently. However, this saves
	 * us the trouble of releasing the same handle multiple times in the loop and once more in the end.
	 */
	for (rptr = grlist, reg_count = 0; NULL != rptr; rptr = rptr->fPtr, reg_count++)
		;
	handles = (gtmcrypt_key_t *)malloc(SIZEOF(gtmcrypt_key_t) * reg_count);
	memset(handles, 0, SIZEOF(gtmcrypt_key_t) * reg_count);
	for (i = 0, rptr = grlist; NULL != rptr; rptr = rptr->fPtr, i++)
	{
		reg = rptr->reg;
		db_name_len = reg->dyn.addr->fname_len;
		db_name = (char *)reg->dyn.addr->fname;
		/* Initialize encryption once. */
		if (grlist == rptr)
		{
			INIT_PROC_ENCRYPTION(NULL, gtmcrypt_errno);
			if (0 != gtmcrypt_errno)
			{
				GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, gtm_putmsg, db_name_len, db_name);
				EXIT_MUPIP_REORG(ERR_MUNOACTION);
			}
		}
		/* Obtain the hash of the specified key and initialize an encryption handle for it. It is somewhat wasteful to do it
		 * for every region given that we only need one hash and the handle will also be shared among all regions, but, as
		 * explained above, this ensures that the operation is provisioned for every database in the encryption
		 * configuration file.
		 */
		GTMCRYPT_HASH_GEN(NULL, db_name_len, db_name, key_len, key, hash, gtmcrypt_errno);
		if (0 != gtmcrypt_errno)
		{
			GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, gtm_putmsg, db_name_len, db_name);
			EXIT_MUPIP_REORG(ERR_MUNOACTION);
		}
		GTMCRYPT_INIT_BOTH_CIPHER_CONTEXTS(NULL, hash, db_name_len, db_name, handles[i], gtmcrypt_errno);
		if (0 != gtmcrypt_errno)
		{
			GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, gtm_putmsg, db_name_len, db_name);
			EXIT_MUPIP_REORG(ERR_MUNOACTION);
		}
	}
	assert(DBKEYSIZE(MAX_KEY_SZ) == gv_keysize);	/* no need to invoke GVKEYSIZE_INIT_IF_NEEDED macro */
	gv_target = targ_alloc(gv_keysize, NULL, NULL);	/* t_begin needs this initialized */
	gv_target_list = NULL;
	memset(&alt_hist, 0, SIZEOF(alt_hist));	/* null-initialize history */
	blkhist = &alt_hist.h[0];
	bptr = NULL;
	bptr_size = 0;
	bml_lcl_buff = NULL;
	/* Start processing regions one by one. */
	for (i = 0, rptr = grlist; NULL != rptr; rptr = rptr->fPtr, i++)
	{
		if (mu_ctrly_occurred || mu_ctrlc_occurred)
			break;
		reg_status = SS_NORMAL;
		reg = rptr->reg;
		if (i != 0)
			util_out_print("", TRUE);
		util_out_print("Region !AD : MUPIP REORG ENCRYPT started", TRUE, REG_LEN_STR(reg));
		if (reg_cmcheck(reg))
		{
			util_out_print("Region !AD : MUPIP REORG ENCRYPT cannot run across network", TRUE, REG_LEN_STR(reg));
			status = reg_status = ERR_MUNOFINISH;
			continue;
		}
		mu_reorg_process = TRUE;	/* gvcst_init will use this value to use gtm_poollimit settings. */
		gvcst_init(reg, NULL);
		mu_reorg_process = FALSE;
		/* Note that db_init() does not release the access-control semaphore in case of MUPIP REORG -ENCRYPT (as determined
		 * based on the mu_reorg_encrypt_in_prog variable), so no need to obtain it here.
		 */
		assert((ftok_sem_reg == reg) && (TRUE == FILE_INFO(reg)->grabbed_ftok_sem));
		TP_CHANGE_REG(reg);	/* sets gv_cur_region, cs_addrs, cs_data, which are needed by jnl_ensure_open and wcs_flu */
		csa = cs_addrs;
		csd = cs_data;
		cnl = csa->nl;
		db_name_len = reg->dyn.addr->fname_len;
		db_name = (char *)reg->dyn.addr->fname;
		/* Access method stored in global directory and database file header might be different, in which case the database
		 * setting prevails.
		 */
		if (dba_bg != REG_ACC_METH(reg))
		{
			CONTINUE_TO_NEXT_REGION(csa, csd, cnl, reg, reg_status, status,
				REORG_IN_PROG_NOT_SET, NOT_HOLDING_CRIT, IS_ERROR,
				util_out_print, ("Region !AD : MUPIP REORG -ENCRYPT cannot continue as access method is not BG",
				TRUE, REG_LEN_STR(reg)));
		}
		/* The mu_getlst call above uses insert_region to create the grlist, which ensures that duplicate regions mapping to
		 * the same db file correspond to only one grlist entry.
		 */
		assert(FALSE == reg->was_open);
		if (reg->read_only)
		{
			CONTINUE_TO_NEXT_REGION(csa, csd, cnl, reg, reg_status, status, REORG_IN_PROG_NOT_SET, NOT_HOLDING_CRIT,
				IS_ERROR, gtm_putmsg_csa, (CSA_ARG(csa) VARLSTCNT(4) ERR_DBRDONLY, 2, db_name_len, db_name));
		}
		/* ++++++++++++++++++++++++++ IN CRIT ++++++++++++++++++++++++++ */
		grab_crit(reg);
		if (!csd->fully_upgraded)
		{
			CONTINUE_TO_NEXT_REGION(csa, csd, cnl, reg, reg_status, status,
				REORG_IN_PROG_NOT_SET, HOLDING_CRIT, IS_ERROR,
				gtm_putmsg_csa, (CSA_ARG(csa) VARLSTCNT(4) ERR_MUREENCRYPTV4NOALLOW, 2, DB_LEN_STR(reg)));
		}
		if (cnl->mupip_extract_count)
		{
			CONTINUE_TO_NEXT_REGION(csa, csd, cnl, reg, reg_status, status,
				REORG_IN_PROG_NOT_SET, HOLDING_CRIT, IS_ERROR,
				gtm_putmsg_csa, (CSA_ARG(csa) VARLSTCNT(8) ERR_ENCRYPTCONFLT, 6,
					RTS_ERROR_LITERAL("MUPIP REORG -ENCRYPT"), REG_LEN_STR(reg), DB_LEN_STR(reg)));
		}
		pid = cnl->reorg_encrypt_pid;
		if (pid && is_proc_alive(pid, 0))
		{
			CONTINUE_TO_NEXT_REGION(csa, csd, cnl, reg, reg_status, status,
				REORG_IN_PROG_NOT_SET, HOLDING_CRIT, IS_ERROR,
				util_out_print, ("Region !AD : MUPIP REORG -ENCRYPT processes cannot operate concurrently. "
				"Concurrent REORG's pid is !UL", TRUE, REG_LEN_STR(reg), pid));
		}
		if ((UNSTARTED == csd->encryption_hash_cutoff) && (0 != csd->encryption_hash2_start_tn))
		{
			CONTINUE_TO_NEXT_REGION(csa, csd, cnl, reg, reg_status,
				status, REORG_IN_PROG_NOT_SET, HOLDING_CRIT, IS_ERROR,
				util_out_print, ("Region !AD : A previous MUPIP REORG -ENCRYPT has finished, but (re)encryption has"
				" not been marked complete. Run MUPIP SET -ENCRYPTIONCOMPLETE to do so", TRUE, REG_LEN_STR(reg)));
		}
		cnl->reorg_encrypt_pid = process_id;
		is_encrypted = csd->is_encrypted;
		if (IS_ENCRYPTED(is_encrypted))
		{
			if ((!TO_BE_ENCRYPTED(is_encrypted) || (UNSTARTED == csd->encryption_hash_cutoff))
					&& !memcmp(hash, csd->encryption_hash, GTMCRYPT_HASH_LEN))
			{
				CONTINUE_TO_NEXT_REGION(csa, csd, cnl, reg, reg_status, status, REORG_IN_PROG_SET, HOLDING_CRIT,
					IS_NOT_ERROR, util_out_print, ("Region !AD : Data already encrypted with the desired key - "
					"no change made", TRUE, REG_LEN_STR(reg)));
			}
#			ifdef DEBUG
			/* In case the database is at all encrypted now, we will need the encryption handle to decrypt existing
			 * blocks. It should have been set up by gvcst_init(). Assert that.
			 */
			assert(NULL != csa->encr_key_handle);
			GTMCRYPT_HASH_CHK(csa, csd->encryption_hash, db_name_len, db_name, gtmcrypt_errno);
			assert(0 == gtmcrypt_errno);
#			endif
		} else if (!TO_BE_ENCRYPTED(is_encrypted))
		{
			CONTINUE_TO_NEXT_REGION(csa, csd, cnl, reg, reg_status, status, REORG_IN_PROG_SET, HOLDING_CRIT, IS_ERROR,
				util_out_print, ("Region !AD : MUPIP REORG -ENCRYPT can only operate on encryptable databases",
				TRUE, REG_LEN_STR(reg)));
		}
		/* Wait for all the readers to complete to prevent them from attempting to digest an encrypted block or trying to
		 * decrypt a block with a wrong key in case MUPIP REORG -ENCRYPT has concurrently processed that block.
		 */
		if (!wait_for_concurrent_reads(csa))
		{
			CONTINUE_TO_NEXT_REGION(csa, csd, cnl, reg, reg_status, status, REORG_IN_PROG_SET, HOLDING_CRIT, IS_ERROR,
				util_out_print, ("Region !AD : Timed out waiting for concurrent reads to finish",
				TRUE, REG_LEN_STR(reg)));
		}
		/* Same for writers. Since we are going to switch the journal file, we might as well write the last EPOCH with the
		 * old encryption settings.
		 */
		if (!wcs_flu(WCSFLU_WRITE_EPOCH))
		{
			CONTINUE_TO_NEXT_REGION(csa, csd, cnl, reg, reg_status, status, REORG_IN_PROG_SET, HOLDING_CRIT, IS_ERROR,
				gtm_putmsg_csa, (CSA_ARG(csa) VARLSTCNT(6) ERR_BUFFLUFAILED, 4,
				LEN_AND_LIT("MUPIP REORG ENCRYPT"), db_name_len, db_name));
		}
		curr_tn = csd->trans_hist.curr_tn;
		total_blks = csd->trans_hist.total_blks;
#		ifdef DEBUG
		initial_blk_cnt = total_blks;
#		endif
		blk_size = csd->blk_size;	/* "blk_size" is used by the BLK_FINI macro */
		send_msg_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_MUREENCRYPTSTART, 4, DB_LEN_STR(reg), process_id, &curr_tn);
		if (UNSTARTED == csd->encryption_hash_cutoff)
		{	/* Database is either fully encrypted or unencrypted. Start encryption from the first block. */
			memcpy(csd->encryption_hash2, hash, GTMCRYPT_HASH_LEN);
			csd->encryption_hash2_start_tn = curr_tn;
			start_tn = curr_tn;
			start_blk = 0;
			need_journal_switch = TRUE;
		} else
		{	/* Encryption was already underway when it was stopped. Resume from the first unencrypted block. */
			assert(UNSTARTED < csd->encryption_hash_cutoff);
			assert((0 < csd->encryption_hash2_start_tn) && (curr_tn >= csd->encryption_hash2_start_tn));
			if (memcmp(hash, csd->encryption_hash2, GTMCRYPT_HASH_LEN))
			{
				CONTINUE_TO_NEXT_REGION(csa, csd, cnl, reg, reg_status, status,
					REORG_IN_PROG_SET, HOLDING_CRIT, IS_ERROR,
					util_out_print, ("Region !AD : MUPIP REORG -ENCRYPT process was previously started with a "
					"different hash. Use the same hash to complete the operation.", TRUE, REG_LEN_STR(reg)));
			}
			start_tn = csd->encryption_hash2_start_tn;
			start_blk = csd->encryption_hash_cutoff;
			if (start_blk > total_blks)
				start_blk = total_blks;
			need_journal_switch = FALSE;
		}
		cnl->reorg_encrypt_cycle++;
		csd->encryption_hash_cutoff = start_blk;
		MARK_AS_TO_BE_ENCRYPTED(csd->is_encrypted);
		assert(NULL != csa->encr_ptr);
		COPY_ENC_INFO(csd, csa->encr_ptr, cnl->reorg_encrypt_cycle);
		memcpy(&csa->encr_key_handle2, &handles[i], SIZEOF(gtmcrypt_key_t));
		if (JNL_ENABLED(csd) && need_journal_switch)
			switch_journal_file(csa, csd);
		DBG_RECORD_CRYPT_UPDATE(csd, csa, cnl, process_id);
		/* Before releasing crit, flush the file header to disk. */
		if (!wcs_flu(WCSFLU_FLUSH_HDR))	/* wcs_flu assumes gv_cur_region is set (which it is in this routine) */
		{
			CONTINUE_TO_NEXT_REGION(csa, csd, cnl, reg, reg_status, status, REORG_IN_PROG_SET, HOLDING_CRIT, IS_ERROR,
				gtm_putmsg_csa, (CSA_ARG(csa) VARLSTCNT(6) ERR_BUFFLUFAILED, 4,
				LEN_AND_LIT("MUPIP REORG ENCRYPT2"), db_name_len, db_name));
		}
		rel_crit(reg);
		release_ftok_semaphore(reg, csa, csd);
		/* -------------------------- OUT OF CRIT -------------------------- */
#		ifdef DEBUG
		if (WBTEST_ENABLED(WBTEST_SLEEP_IN_MUPIP_REORG_ENCRYPT))
		{
			if (2 > gtm_white_box_test_case_count)
			{
				util_out_print("Starting the sleep", TRUE);
				if (0 == gtm_white_box_test_case_count)
				{
					LONG_SLEEP(60);
				}
			} else
				reencryption_count = gtm_white_box_test_case_count;
		}
#		endif
		udi = FILE_INFO(reg);
		if (udi->fd_opened_with_o_direct)
		{
			DIO_BUFF_EXPAND_IF_NEEDED(udi, blk_size, &(TREF(dio_buff)));
		} else if ((NULL != bptr) && (bptr_size < blk_size))
		{	/* malloc/free "bptr" for each region as GDS block-size can be different */
			free(bptr);
			bptr = NULL;
		}
		start_bmp = ROUND_DOWN2(start_blk, BLKS_PER_LMAP);
		last_bmp = ROUND_DOWN2(total_blks - 1, BLKS_PER_LMAP);
		for (curbmp = start_bmp; curbmp <= last_bmp; curbmp += BLKS_PER_LMAP)
		{
			if (mu_ctrly_occurred || mu_ctrlc_occurred)
			{
				reg_status = ERR_MUNOFINISH;
				break;
			}
			assert(!csa->now_crit);
			bml_sm_buff = t_qread(curbmp, (sm_int_ptr_t)&cycle, &cr); /* bring block into the cache outside of crit */
			/* ++++++++++++++++++++++++++ IN CRIT ++++++++++++++++++++++++++ */
			grab_crit_encr_cycle_sync(reg); /* needed so t_qread does not return NULL below */
			/* Safeguard against someone concurrently changing the database file header. It is unsafe to continue. */
			if (start_tn != csd->encryption_hash2_start_tn)
			{
				rel_crit(reg);
				reg_status = ERR_MUNOFINISH;
				break;
			}
			if (total_blks > csd->trans_hist.total_blks)
			{
				total_blks = csd->trans_hist.total_blks;
				last_bmp = ROUND_DOWN2(total_blks - 1, BLKS_PER_LMAP);
				if (curbmp >= total_blks)
				{
					rel_crit(reg);
					assert(SS_NORMAL == reg_status);
					break;
				}
			}
			/* Before changing the hash cutoff, check if the journal file is not open in shared memory (possible
			 * if a concurrent jnl switch happened due to a MUPIP SET JOURNAL or MUPIP BACKUP etc.). If so, open
			 * the journal file in shared memory while the db and jnl headers have identical hash cutoff or else
			 * whoever later opens the journal file first would get a CRYPTJNLMISMATCH error.
			 */
			if (JNL_ENABLED(csd))
			{
				jpc = csa->jnl;
				if (0 == cnl->jnl_file.u.inode)
				{
					assert(JNL_FILE_SWITCHED(jpc));
					ENSURE_JNL_OPEN(csa, reg);
				}
				assert(0 != cnl->jnl_file.u.inode);
			}
			csd->encryption_hash_cutoff = curbmp;
			bml_sm_buff = t_qread(curbmp, (sm_int_ptr_t)&cycle, &cr); /* now that in crit, note down stable buffer */
			if (NULL == bml_sm_buff)
				rts_error_csa(CSA_ARG(csa) VARLSTCNT(1) ERR_DSEBLKRDFAIL);
			/* Take a copy of the shared memory bitmap buffer into process-private memory before releasing crit. We are
			 * interested in those blocks that are currently marked as USED in the bitmap. It is possible that once we
			 * release crit, concurrent updates change the bitmap state of those blocks. In that case, those updates
			 * will take care of doing the (re)encryption of those blocks based on the encryption_hash2_start_tn value.
			 */
			if (NULL == bml_lcl_buff)
				bml_lcl_buff = malloc(BM_SIZE(BLKS_PER_LMAP));
			memcpy(bml_lcl_buff, (blk_hdr_ptr_t)bml_sm_buff, BM_SIZE(BLKS_PER_LMAP));
			if (FALSE == cert_blk(reg, curbmp, (blk_hdr_ptr_t)bml_lcl_buff, 0, FALSE, NULL))
			{	/* Certify the block while holding crit as cert_blk uses fields from file-header (shared memory). */
				rel_crit(reg);
				assert(FALSE);	/* In pro, skip ugprading/downgarding all blks in this unreliable local bitmap. */
				util_out_print("Region !AD : Bitmap Block [0x!XL] has integrity errors. Skipping this bitmap.",
					TRUE, REG_LEN_STR(reg), curbmp);
				status = reg_status = ERR_MUNOFINISH;
				continue;
			}
			rel_crit(reg);
			/* -------------------------- OUT OF CRIT -------------------------- */
			/* ------------------------------------------------------------------------
			 *         (Re)encrypt all BUSY and REUSABLE blocks in the current bitmap
			 * ------------------------------------------------------------------------
			 */
			mapsize = (curbmp == last_bmp) ? (total_blks - curbmp) : BLKS_PER_LMAP;
			assert(0 != mapsize);
			for (lcnt = 0; lcnt < mapsize; lcnt++)
			{
				if (mu_ctrly_occurred || mu_ctrlc_occurred)
				{
					reg_status = ERR_MUNOFINISH;
					break;
				}
				GET_BM_STATUS(bml_lcl_buff, lcnt, bml_status);
				assert(BLK_MAPINVALID != bml_status); /* cert_blk ran clean so we don't expect invalid entries */
				if (BLK_FREE == bml_status)
					continue;
				curblk = curbmp + lcnt;
				if (lcnt)
				{	/* non-bitmap block */
					/* read in block from disk into private buffer. don't pollute the cache yet */
					if (!udi->fd_opened_with_o_direct)
					{
						if (NULL == bptr)
						{
							bptr = (sm_uc_ptr_t)malloc(blk_size);
							bptr_size = blk_size;
						}
						buff = bptr;
					} else
						buff = (sm_uc_ptr_t)(TREF(dio_buff)).aligned;
					mu_reorg_encrypt_in_prog = MUPIP_REORG_IN_PROG_LOCAL_DSK_READ;
					status1 = dsk_read(curblk, buff, NULL, FALSE);
					mu_reorg_encrypt_in_prog = MUPIP_REORG_IN_PROG_TRUE;
					if (SS_NORMAL != status1)
					{
						gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), status1);
						util_out_print("Region !AD : Error occurred while reading block [0x!XL]",
								TRUE, REG_LEN_STR(reg), curblk);
						reg_status = ERR_MUNOFINISH;
						break;
					}
					blk_tn = ((blk_hdr_ptr_t)buff)->tn;
					if (blk_tn >= start_tn)
						continue;
				}
				/* Begin non-TP transaction to (re)encrypt the block.
				 * The way we do that is by updating the block using a null update array.
				 * Any update to a block will trigger an automatic (re)encryption of the block based on
				 * 	the current fileheader's encryption_hash2_start_tn setting.
				 */
				t_begin(ERR_MUREORGFAIL, UPDTRNS_DB_UPDATED_MASK);
				for (; ;)
				{
					CHECK_AND_RESET_UPDATE_ARRAY;	/* reset update_array_ptr to update_array */
					curr_tn = csd->trans_hist.curr_tn;
					blkhist->cse = NULL;	/* start afresh (do not use value from previous retry) */
					if ((CDB_STAGNATE == t_tries) && (curblk >= csd->trans_hist.total_blks))
					{	/* Possible in case of a concurrent reorg truncate.
						 * In this case, the reorg encrypt is done.
						 * Set variables so we fall out of the nested for loops.
						 */
						assert(csa->now_crit);
						total_blks = csd->trans_hist.total_blks;
						last_bmp = ROUND_DOWN2(total_blks - 1, BLKS_PER_LMAP);
						assert(NULL == reorg_encrypt_restart_csa);
						t_abort(reg, csa);
						assert(!csa->now_crit);	/* "t_abort" should have released crit */
						assert(SS_NORMAL == reg_status);
						curbmp = last_bmp; /* ensure we "break" out of 1st level (outermost) for loop */
						lcnt = mapsize;	/* to break out of 2nd level (outer) for-loop */
						break;
					}
					blkBase = t_qread(curblk, (sm_int_ptr_t)&blkhist->cycle, &blkhist->cr);
					if (NULL == blkBase)
					{
						t_retry((enum cdb_sc)rdfail_detail);
						continue;
					}
					blkhist->blk_num = curblk;
					blkhist->buffaddr = blkBase;
					new_hdr = *(blk_hdr_ptr_t)blkBase;
					blk_tn = new_hdr.tn;
					inctn_opcode = inctn_blkreencrypt;
					inctn_detail.blknum_struct.blknum = curblk;
					if (!lcnt)
					{	/* Means a bitmap block. */
						BLK_ADDR(blkid_ptr, SIZEOF(block_id), block_id);
						*blkid_ptr = 0;
						t_write_map(blkhist, (unsigned char *)blkid_ptr, curr_tn, 0);
						assert(&alt_hist.h[0] == blkhist);
						alt_hist.h[0].blk_num = 0; /* create empty history for bitmap block */
						assert(update_trans);
					} else
					{	/* Non-bitmap block. Fill in history for validation in t_end */
						assert(curblk);	/* we should never come here for block 0 (bitmap) */
						assert(blkhist->blk_num == curblk);
						assert(blkhist->buffaddr == blkBase);
						blkhist->tn = curr_tn;
						alt_hist.h[1].blk_num = 0;
						/* Also need to pass the bitmap as history to detect if any concurrent M-kill
						 * is freeing up the same USED block that we are trying to (re)encrypt OR if any
						 * concurrent M-set is reusing the same RECYCLED block that we are trying to
						 * (re)encrypt. Because of t_end currently not being able to validate a bitmap
						 * without that simultaneously having a cse, we need to create a cse for the
						 * bitmap that is used only for bitmap history validation, but should not be
						 * used to update the contents of the bitmap block in bg_update.
						 */
						bmlhist.buffaddr = t_qread(curbmp, (sm_int_ptr_t)&bmlhist.cycle, &bmlhist.cr);
						if (NULL == bmlhist.buffaddr)
						{
							t_retry((enum cdb_sc)rdfail_detail);
							continue;
						}
						bmlhist.blk_num = curbmp;
						bmlhist.tn = curr_tn;
						GET_BM_STATUS(bmlhist.buffaddr, lcnt, bml_status);
						if (BLK_MAPINVALID == bml_status)
						{
							t_retry(cdb_sc_lostbmlcr);
							continue;
						}
						if ((BLK_FREE != bml_status) && (blk_tn < start_tn))
						{	/* Block still needs to be (re)encrypted; create cse. */
							/* TODO: See if we can avoid the full-blown block write and instead make
							 * "t_end" or "bg_update_phase1" only bump the tn but otherwise leave the
							 * block alone.
							 */
							BLK_INIT(bs_ptr, bs1);
							BLK_SEG(bs_ptr, blkBase + SIZEOF(new_hdr),
								new_hdr.bsiz - SIZEOF(new_hdr));
							BLK_FINI(bs_ptr, bs1);
							t_write(blkhist, (unsigned char *)bs1, 0, 0,
								((blk_hdr_ptr_t)blkBase)->levl, FALSE,
								FALSE, GDS_WRITE_PLAIN);
							/* The directory tree status for now is only used to determine whether
							 * writing the block to snapshot file (see t_end_sysops.c). For
							 * REORG -ENCRYPT process, the block is updated in a sequential way without
							 * changing the gv_target. In this case, we assume the block is in directory
							 * tree so as to have it written to the snapshot file.
							 */
							BIT_SET_DIR_TREE(cw_set[cw_set_depth - 1].blk_prior_state);
							/* Reset update_trans in case previous retry had set it to 0 */
							update_trans = UPDTRNS_DB_UPDATED_MASK;
							if (BLK_RECYCLED == bml_status)
							{
								assert(cw_set[cw_set_depth - 1].mode == gds_t_write);
								cw_set[cw_set_depth - 1].mode = gds_t_write_recycled;
								/* We SET block as NOT RECYCLED, otherwise, the mm_update()
								 * or bg_update_phase2 may skip writing it to snapshot file
								 * when its level is 0
								 */
								BIT_CLEAR_RECYCLED(cw_set[cw_set_depth - 1].blk_prior_state);
							}
						} else
						{	/* Block got (re)encrypted by another process since we did the dsk_read or
							 * this block became marked free in the bitmap. No need to update this
							 * block; just call t_end for validation of both the non-bitmap block as
							 * well as the bitmap block. Note down that this transaction is no longer
							 * updating any blocks.
							 */
							update_trans = 0;
						}
						/* Need to put bit maps on the end of the cw set for concurrency checking.
						 * We want to simulate t_write_map, except we want to update "cw_map_depth"
						 * instead of "cw_set_depth". Hence the save and restore logic below.
						 * This part of the code is similar to the one in mu_swap_blk.c
						 */
						save_cw_set_depth = cw_set_depth;
						assert(!cw_map_depth);
						t_write_map(&bmlhist, NULL, curr_tn, 0); /* will increment cw_set_depth */
						cw_map_depth = cw_set_depth; /* set cw_map_depth to latest cw_set_depth */
						cw_set_depth = save_cw_set_depth;/* restore cw_set_depth */
						/* t_write_map simulation end */
					}
					assert(SIZEOF(lcl_update_trans) == SIZEOF(update_trans));
					lcl_update_trans = update_trans;	/* take a copy before t_end modifies it */
					if ((trans_num)0 != t_end(&alt_hist, NULL, TN_NOT_SPECIFIED))
					{
#						ifdef DEBUG
						if (WBTEST_ENABLED(WBTEST_SLEEP_IN_MUPIP_REORG_ENCRYPT)
								&& (2 <= gtm_white_box_test_case_count))
						{
							reencryption_count--;
							if (0 == reencryption_count)
							{
								reg_status = ERR_MUNOFINISH;
								break;
							}
						}
#						endif
						assert(csd == cs_data);
						if (!lcl_update_trans)
							assert(lcnt);
						break;
					}
					assert(csd == cs_data);
				}
#				ifdef DEBUG
				if (SS_NORMAL != reg_status)
					break;	/* this takes into account the WBTEST_SLEEP_IN_MUPIP_REORG_ENCRYPT case above */
#				endif
			}
			if (SS_NORMAL != reg_status)
				break;
		}
		if (SS_NORMAL == reg_status)
		{
			get_ftok_semaphore(reg, csa);
			grab_crit(reg);
			/* Wait for all the readers to complete to prevent them from attempting to digest an
			 * encrypted block or decrypt a block with a wrong key in case MUPIP REORG -ENCRYPT has
			 * concurrently processed that block.
			 */
			if (!wait_for_concurrent_reads(csa))
			{
				util_out_print("Region !AD : Timed out waiting for concurrent reads to finish2",
						TRUE, REG_LEN_STR(reg));
				reg_status = ERR_MUNOFINISH;
				break;
			}
			/* Same for writers. */
			if (!wcs_flu(WCSFLU_WRITE_EPOCH))
			{
				gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_BUFFLUFAILED, 4,
						LEN_AND_LIT("MUPIP REORG ENCRYPT3"), db_name_len, db_name);
				reg_status = ERR_MUNOFINISH;
				break;
			}
			/* We are not resetting encryption_hash2_start_tn because we do not want the
			 * database to be reencryptable before the user had a chance to back it up.
			 */
			csd->encryption_hash_cutoff = UNSTARTED;
			csd->non_null_iv = TRUE;
			SET_AS_ENCRYPTED(csd->is_encrypted);
			memcpy(csd->encryption_hash, csd->encryption_hash2, GTMCRYPT_RESERVED_HASH_LEN);
			memset(csd->encryption_hash2, 0, GTMCRYPT_RESERVED_HASH_LEN);
			/* A simple copy because gtmcrypt_key_t is a pointer type. */
			csa->encr_key_handle = csa->encr_key_handle2;
			csa->encr_key_handle2 = NULL;
			cnl->reorg_encrypt_cycle++;
			assert(NULL != csa->encr_ptr);
			COPY_ENC_INFO(csd, csa->encr_ptr, cnl->reorg_encrypt_cycle;);
			if (JNL_ENABLED(csd))
				switch_journal_file(csa, csd);
			DBG_RECORD_CRYPT_UPDATE(csd, csa, cnl, process_id);
		}
		/* We are done (although potentially due to an error or a Ctrl-C), so update file-header fields to store reorg's
		 * progress before exiting.
		 */
		if (!csa->now_crit)
		{
			get_ftok_semaphore(reg, csa);
			grab_crit(reg);
		}
		assert(csa->now_crit);
		assert(UNSTARTED == csd->encryption_hash_cutoff || (SS_NORMAL != reg_status));
		if (start_tn != csd->encryption_hash2_start_tn)
		{	/* csd->encryption_hash2_start_tn changed since reorg started. discontinue the reorg */
			util_out_print("Region !AD : Starting tn number changed during REORG (expected 0x!16@XQ but got 0x!16@XQ). "
					"Stopping REORG.", TRUE, REG_LEN_STR(reg), &csd->encryption_hash2_start_tn, start_tn);
			reg_status = ERR_MUNOFINISH;
		} else
		{
			/* Flush all changes noted down in the file-header. */
			if (!wcs_flu(WCSFLU_FLUSH_HDR))	/* wcs_flu assumes gv_cur_region is set (which it is in this routine) */
			{
				gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_BUFFLUFAILED, 4,
					LEN_AND_LIT("MUPIP REORG ENCRYPT4"), db_name_len, db_name);
				reg_status = ERR_MUNOFINISH;
			}
		}
		curr_tn = csd->trans_hist.curr_tn;
		cnl->reorg_encrypt_pid = 0;
		rel_crit(reg);
		release_ftok_semaphore(reg, csa, csd);
		/* Issue success or failure message for this region */
		if (SS_NORMAL == reg_status)
		{
			util_out_print("Region !AD : Database is now FULLY ENCRYPTED with the following key: !AD",
					TRUE, REG_LEN_STR(reg), key_len, key);
			util_out_print("Region !AD : MUPIP REORG ENCRYPT finished", TRUE, REG_LEN_STR(reg));
			send_msg_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_MUREENCRYPTEND, 4, DB_LEN_STR(reg), process_id, &curr_tn);
		} else
		{
			assert(ERR_MUNOFINISH == reg_status);
			util_out_print("Region !AD : MUPIP REORG ENCRYPT incomplete. See above messages.",
					TRUE, REG_LEN_STR(reg));
			status = reg_status;
		}
	}
	if (NULL != handles)
		free(handles);
	if (NULL != bptr)
		free(bptr);
	if (NULL != bml_lcl_buff)
		free(bml_lcl_buff);
	if (mu_ctrly_occurred || mu_ctrlc_occurred)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_REORGCTRLY);
		status = ERR_MUNOFINISH;
	}
	EXIT_MUPIP_REORG(status);
}

/* This code is similar to that in wcs_recover.c and should be merged eventually. */
boolean_t wait_for_concurrent_reads(sgmnt_addrs *csa)
{
	uint4			stuck_cnt, blocking_pid;
	int4			total_stuck_cnt_left;
	cache_rec_ptr_t		cr, cr_top;
	sgmnt_data_ptr_t	csd;

	total_stuck_cnt_left = MAX_WAIT_FOR_RIP * BUF_OWNER_STUCK;
	assert(0 < total_stuck_cnt_left); /* safety net just in case the macro values grow and we overflow a signed int */
	csd = csa->hdr;
	assert(csa == cs_addrs);
	assert(csd == cs_data);
	assert(csa->now_crit);
	cr = (cache_rec_ptr_t)csa->acc_meth.bg.cache_state->cache_array + csd->bt_buckets;
	for (cr_top = cr + csd->n_bts; cr < cr_top; cr++)
	{
		for (stuck_cnt = 1; -1 != cr->read_in_progress; stuck_cnt++, total_stuck_cnt_left--)
		{
			blocking_pid = cr->r_epid;
			assert(process_id != blocking_pid);
			if ((BUF_OWNER_STUCK < stuck_cnt) || (0 > total_stuck_cnt_left))
			{
				if (0 != blocking_pid)
					GET_C_STACK_FROM_SCRIPT("BUFOWNERSTUCK", process_id, blocking_pid, stuck_cnt);
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_BUFRDTIMEOUT, 6, process_id,
					 cr->blk, cr, blocking_pid, DB_LEN_STR(csa->region));
				return FALSE;
			}
			if ((0 != blocking_pid) && is_proc_alive(blocking_pid, 0))
			{	/* Kickstart the process taking a long time in case it was suspended */
				UNIX_ONLY(continue_proc(blocking_pid));
			}
			wcs_sleep(stuck_cnt);
		}
	}
	return TRUE;
}

void get_ftok_semaphore(gd_region *reg, sgmnt_addrs *csa)
{
	unix_db_info *udi;

	udi = FILE_INFO(reg);
	if (!ftok_sem_lock(reg, FALSE))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_CRITSEMFAIL, 2, LEN_AND_STR(udi->fn));
	assert(ftok_sem_reg == reg);
}

void release_ftok_semaphore(gd_region *reg, sgmnt_addrs *csa, sgmnt_data_ptr_t csd)
{
	unix_db_info *udi;

	udi = FILE_INFO(reg);
	/* Second parameter decr_cnt is FALSE because we will decrement the counter semaphore as part of gds_rundown. This is just
	 * releasing the ftok in the middle of the reorg encrypt process and we do not want to modify the counter in those cases.
	 */
	if (!ftok_sem_release(reg, FALSE, FALSE))
		rts_error_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_DBFILERR, 2, DB_LEN_STR(reg));
	FTOK_TRACE(csa, csd->trans_hist.curr_tn, ftok_ops_release, process_id);
	udi->grabbed_ftok_sem = FALSE;
}

void switch_journal_file(sgmnt_addrs *csa, sgmnt_data_ptr_t csd)
{
	jnl_private_control	*jpc;
	jnl_buffer_ptr_t	jbp;
	uint4			jnl_status;

	assert(csa->now_crit);
	SET_GBL_JREC_TIME; /* jnl_ensure_open/jnl_file_extend and its callees assume jgbl.gbl_jrec_time is set */
	jpc = csa->jnl;
	/* Before writing to jnlfile, adjust jgbl.gbl_jrec_time if needed to maintain time order of jnl records. This needs to be
	 * done BEFORE the jnl_ensure_open as that could write journal records (if it decides to switch to a new journal file).
	 */
	jbp = jpc->jnl_buff;
	ADJUST_GBL_JREC_TIME(jgbl, jbp);
	jnl_status = jnl_ensure_open(gv_cur_region, csa);
	if (0 == jnl_status)
	{
		if (EXIT_ERR == SWITCH_JNL_FILE(jpc))
			rts_error_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_JNLEXTEND, 2, JNL_LEN_STR(csd));
	} else
	{
		if (SS_NORMAL != jpc->status)
			rts_error_csa(CSA_ARG(csa) VARLSTCNT(7) jnl_status, 4, JNL_LEN_STR(csd),
					DB_LEN_STR(gv_cur_region), jpc->status);
		else
			rts_error_csa(CSA_ARG(csa) VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(csd),
					DB_LEN_STR(gv_cur_region));
	}
}
