/****************************************************************
 *								*
 * Copyright (c) 2001-2025 Fidelity National Information	*
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
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "error.h"
#include "filestruct.h"
#include "gdsblk.h"
#include "gdsbml.h"
#include "jnl.h"		/* needed for WCSFLU_* macros */
#include "sleep_cnt.h"
#include "util.h"
#include "gvcst_protos.h"	/* for gvcst_init prototype */
#include "change_reg.h"
#include "gtmmsg.h"
#include "mu_int_wait_rdonly.h"
#include "interlock.h"
#include "gtmcrypt.h"
#include "db_snapshot.h"
#include "gt_timer.h"
#include "mupint.h"
#include "wbox_test_init.h"
#include "inline_atomic_pid.h" /* for DEFINE_ATOMIC_OP(gtm_atomic_uint, ATOMIC_LOAD, memory_order_acquire) */
#include "inline_not_frozen.h" /* for not_frozen_hard */

#define MUPIP_INTEG "MUPIP INTEG"

GBLREF boolean_t		ointeg_this_reg;
GBLREF gd_region		*gv_cur_region;
GBLREF sgmnt_data		mu_int_data;
GBLREF unsigned char		*mu_int_master;
GBLREF uint4			mu_int_skipreg_cnt;
GBLREF enc_handles		mu_int_encr_handles;
GBLREF sgmnt_addrs		*cs_addrs;
GBLREF boolean_t		jnlpool_init_needed, online_specified, preserve_snapshot;
GBLREF util_snapshot_ptr_t	util_ss_ptr;
#ifdef DEBUG
GBLREF uint4			process_id;
#endif

LITREF	char			*gtm_dbversion_table[];

error_def(ERR_BUFFLUFAILED);
error_def(ERR_DBUPGRDREQ);
error_def(ERR_SSMMNOALLOW);

void mu_int_reg(gd_region *reg, boolean_t *return_value, boolean_t return_after_open)
{
	boolean_t		read_only, was_crit;
	freeze_status		status;
	node_local_ptr_t	cnl;
	sgmnt_addrs     	*csa;
	sgmnt_data_ptr_t	csd;
	sgmnt_data		*csd_copy_ptr;
	gd_segment		*seg;
	int			gtmcrypt_errno;
#	ifdef DEBUG
	boolean_t		need_to_wait = FALSE;
	int			trynum;
	uint4			curr_wbox_seq_num;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	*return_value = FALSE;
	jnlpool_init_needed = TRUE;
	ESTABLISH(mu_int_reg_ch);
	if (dba_usr == reg->dyn.addr->acc_meth)
	{
		util_out_print("!/Can't integ region !AD; not GDS format", TRUE,  REG_LEN_STR(reg));
		mu_int_skipreg_cnt++;
		return;
	}
	gv_cur_region = reg;
	if (reg_cmcheck(reg))
	{
		util_out_print("!/Can't integ region across network", TRUE);
		mu_int_skipreg_cnt++;
		return;
	}
	gvcst_init(gv_cur_region, NULL);
	if (gv_cur_region->was_open)
	{	/* already open under another name */
		gv_cur_region->open = FALSE;
		return;
	}
	if (return_after_open)
	{
		*return_value = TRUE;
		return;
	}
	change_reg();
	csa = &FILE_INFO(gv_cur_region)->s_addrs;
	cnl = csa->nl;
	csd = csa->hdr;
	read_only = gv_cur_region->read_only;
	assert(NULL != mu_int_master);
	/* Ensure that we don't see an increase in the file header and master map size compared to it's maximum values */
	assert(SGMNT_HDR_LEN >= SIZEOF(sgmnt_data) && (MASTER_MAP_SIZE_MAX >= MASTER_MAP_SIZE(csd)));
	if (!csd->fully_upgraded)
	{	/* V7 dropped support for V4 along with the prior UPGRADE/DOWNGRADE capabilities. If the desired DB
		 * format is prior to V6p (internal only format), reject any operation with the DB */
		if (BLK_ID_32_VER > csd->desired_db_format)
		{
			gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_DBUPGRDREQ, 3, DB_LEN_STR(gv_cur_region),
					gtm_dbversion_table[csd->desired_db_format]);
			mu_int_skipreg_cnt++;
			return;
		} /* else V6p+ is not be a problem */
	}
	if (!ointeg_this_reg || read_only)
	{
		TREF(integ_cannotskip_crit) = TRUE;
		status = region_freeze(gv_cur_region, TRUE, FALSE, TRUE, FALSE, !read_only);
		switch (status)
		{
			case REG_ALREADY_FROZEN:
				if (csa->read_only_fs)
					break;
				util_out_print("!/Database for region !AD is already frozen, not integing",
					TRUE, REG_LEN_STR(gv_cur_region));
				mu_int_skipreg_cnt++;
				return;
			case REG_FLUSH_ERROR:
				gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_BUFFLUFAILED, 4, LEN_AND_LIT(MUPIP_INTEG),
					DB_LEN_STR(gv_cur_region));
				mu_int_skipreg_cnt++;
				return;
			case REG_HAS_KIP:
				/* We have already waited for KIP to reset. This time do not wait for KIP */
				status = region_freeze(gv_cur_region, TRUE, FALSE, FALSE, FALSE, !read_only);
				if (REG_ALREADY_FROZEN == status)
				{
					if (csa->read_only_fs)
						break;
					util_out_print("!/Database for region !AD is already frozen, not integing",
						TRUE, REG_LEN_STR(gv_cur_region));
					mu_int_skipreg_cnt++;
					return;
				} else if (REG_FLUSH_ERROR == status)
				{
					gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_BUFFLUFAILED, 4, LEN_AND_LIT(MUPIP_INTEG),
						DB_LEN_STR(gv_cur_region));
					mu_int_skipreg_cnt++;
					return;
				}
				assert(REG_FREEZE_SUCCESS == status);
				/* no break */
			case REG_FREEZE_SUCCESS:
				break;
			default:
				assert(FALSE);
				/* no break */
		}
		if (read_only && (dba_bg == csa->hdr->acc_meth) && !mu_int_wait_rdonly(csa, MUPIP_INTEG))
		{
			mu_int_skipreg_cnt++;
			return;
		}
	}
	if (!ointeg_this_reg)
	{	/* Take a copy of the file-header. To ensure it is consistent, do it while holding crit. */
		was_crit = csa->now_crit;
		if (!was_crit)
			grab_crit(gv_cur_region, WS_6);
		memcpy((uchar_ptr_t)&mu_int_data, (uchar_ptr_t)csd, SIZEOF(sgmnt_data));
		if (!was_crit)
			rel_crit(gv_cur_region);
		memcpy(mu_int_master, MM_ADDR(csd), MASTER_MAP_SIZE(csd));
		csd_copy_ptr = &mu_int_data;
	} else
	{
		if (!ss_initiate(gv_cur_region, util_ss_ptr, &csa->ss_ctx, preserve_snapshot, MUPIP_INTEG))
		{
			mu_int_skipreg_cnt++;
			assert(NULL != csa->ss_ctx);
			ss_release(&csa->ss_ctx);
			ointeg_this_reg = FALSE; /* Turn off ONLINE INTEG for this region */
			assert(process_id != cnl->in_crit); /* Ensure ss_initiate released the crit before returning */
			assert(not_frozen_hard(csa)); /* Ensure region is unfrozen before returning from ss_initiate */
			assert(INTRPT_IN_SS_INITIATE != intrpt_ok_state); /* Ensure ss_initiate released intrpt_ok_state */
			return;
		}
		assert(process_id != cnl->in_crit); /* Ensure ss_initiate released the crit before returning */
		assert(INTRPT_IN_SS_INITIATE != intrpt_ok_state); /* Ensure ss_initiate released intrpt_ok_state */
		csd_copy_ptr = &csa->ss_ctx->ss_shm_ptr->shadow_file_header;
#		if defined(DEBUG)
		curr_wbox_seq_num = 1;
		cnl->wbox_test_seq_num = curr_wbox_seq_num; /* indicate we took the next step */
		GTM_WHITE_BOX_TEST(WBTEST_OINTEG_WAIT_ON_START, need_to_wait, TRUE);
		if (need_to_wait) /* wait for them to take next step */
		{
			trynum = 30; /* given 30 cycles to tell you to go */
			while ((curr_wbox_seq_num == cnl->wbox_test_seq_num) && trynum--)
				LONG_SLEEP(1);
			cnl->wbox_test_seq_num++; /* let them know we took the next step */
			assert(trynum);
		}
#		endif
	}
	if (USES_ANY_KEY(csd_copy_ptr))
	{ 	/* Initialize mu_int_encrypt_key_handle to be used in mu_int_read */
		seg = gv_cur_region->dyn.addr;
		INIT_DB_OR_JNL_ENCRYPTION(&mu_int_encr_handles, csd_copy_ptr, seg->fname_len, (char *)seg->fname, gtmcrypt_errno);
		if (0 != gtmcrypt_errno)
		{
			GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, gtm_putmsg, seg->fname_len, seg->fname);
			mu_int_skipreg_cnt++;
			return;
		}
	}
	*return_value = mu_int_fhead();
	REVERT;
	return;
}
