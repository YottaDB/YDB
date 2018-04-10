/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_statvfs.h"

#include <errno.h>
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_stat.h"
#include "gtm_string.h"
#include "util.h"

#include "gtm_rename.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "eintr_wrappers.h"
#include "gtmmsg.h"
#include "iosp.h"	/* for SS_NORMAL */
#include "gtmio.h"
#include "sgtm_putmsg.h"
#include "repl_sp.h"	/* for F_CLOSE used by the JNL_FD_CLOSE macro */
#include "interlock.h"
#include "lockconst.h"
#include "aswp.h"
#include "is_file_identical.h"
#include "jnl_file_close_timer.h"
#ifdef __MVS__
#include "gtm_zos_io.h"
#endif

#ifdef	GTM_FD_TRACE
#include "gtm_dbjnl_dupfd_check.h"
#endif

#include "wbox_test_init.h"

#define SET_JPC_ERR_STR(ERR1, ERR2, BUF)							\
{												\
	jpc->status = ERR2;									\
	sts = ERR1;										\
	sgtm_putmsg(BUF, VARLSTCNT(7) sts, 4, JNL_LEN_STR(csd), DB_LEN_STR(reg), jpc->status);	\
	jpc->err_str = BUF;									\
}

GBLREF	boolean_t		is_src_server;
GBLREF	boolean_t		mupip_jnl_recover;

error_def(ERR_JNLFILOPN);
error_def(ERR_JNLMOVED);
error_def(ERR_JNLOPNERR);
error_def(ERR_JNLRDERR);

uint4 jnl_file_open(gd_region *reg, boolean_t init)
{
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	jnl_private_control	*jpc;
	jnl_buffer_ptr_t     	jb;
	struct stat		stat_buf;
	uint4			sts;
	sm_uc_ptr_t		nameptr;
	int			fstat_res;
	int			close_res;
	boolean_t		switch_and_retry;
	char			buff[OUT_BUFF_SIZE];
	ZOS_ONLY(int		realfiletag;)

	csa = &FILE_INFO(reg)->s_addrs;
	csd = csa->hdr;
	jpc = csa->jnl;
	jb = jpc->jnl_buff;
	assert(NOJNL == jpc->channel);
	sts = 0;
	jpc->status = jpc->status2 = SS_NORMAL;
	assert(NULL == jpc->err_str);
	nameptr = csd->jnl_file_name;
	assert('/' == csd->jnl_file_name[0]);
	if (init)
	{
		assert(csd->jnl_file_len < JNL_NAME_SIZE);
		/* csd->jnl_file_name[] is of size JNL_NAME_SIZE and must not change across GT.M versions hence the below asserts */
		assert(JNL_NAME_SIZE == ARRAYSIZE(csd->jnl_file_name));
		assert(256 == JNL_NAME_SIZE);
		nameptr[csd->jnl_file_len] = 0;
		cre_jnl_file_intrpt_rename(csa);
		/* although "jnl_file_close" would have reset jnl_file.u.inode and device to 0 and incremented cycle, it
		 * might have got shot in the middle of executing those instructions. we redo it here just to be safe.
		 */
		csa->nl->jnl_file.u.inode = 0;
		csa->nl->jnl_file.u.device = 0;
		jb->cycle++;
		/* Source Server only reads journal files so must never try to create and switch to a new journal file. */
		switch_and_retry = (!is_src_server);
		for (;;)
		{
			/* D9E04-002445 MUPIP RECOVER always open journal file without O_SYNC, ignoring jnl_sync_io */
			if (csd->jnl_sync_io && !mupip_jnl_recover)
			{
				OPENFILE_SYNC_CLOEXEC((sm_c_ptr_t)csd->jnl_file_name, O_RDWR, jpc->channel);
				jpc->sync_io = TRUE;
			} else
			{
				OPENFILE_CLOEXEC((sm_c_ptr_t)csd->jnl_file_name, O_RDWR, jpc->channel);
				jpc->sync_io = FALSE;
			}
			/* Check that if ever open errors out (i.e. return status is FD_INVALID=-1),
			 * jpc->channel will be already set to NOJNL (which is also defined to be -1).
			 */
			assert(FD_INVALID == NOJNL);
			if (FD_INVALID == jpc->channel)
			{
				jpc->status = errno;
				sts = ERR_JNLFILOPN;
				if (EACCES == jpc->status || EROFS == jpc->status)
					break;
			} else
			{
				ZOS_ONLY(gtm_zos_tag_to_policy(jpc->channel, TAG_BINARY, &realfiletag);)
				FSTAT_FILE(jpc->channel, &stat_buf, fstat_res);
				if (-1 == fstat_res)
				{
					jpc->status = errno;
					assert(FALSE);
					sts = ERR_JNLRDERR;
					JNL_FD_CLOSE(jpc->channel, close_res);	/* sets jpc->channel to NOJNL */
					break;
				} else
					sts = jnl_file_open_common(reg, (off_jnl_t) stat_buf.st_size, buff);
			}
#			ifdef DEBUG
			/* Will fail if Source Server would need to switch journal files. */
			assert((gtm_white_box_test_case_enabled && (WBTEST_JNL_SWITCH_EXPECTED == gtm_white_box_test_case_number))
					|| (0 == sts) || (!is_src_server));
#			endif
			if ((0 != sts) && switch_and_retry)
			{	/* Switch to a new journal file and retry, but only once */
				sts = jnl_file_open_switch(reg, sts, buff);
				if (0 == sts)
				{
					switch_and_retry = FALSE;
					continue;
				}
			}
			break;
		}
	} else   /* not init */
	{
		assert(0 == nameptr[csd->jnl_file_len]);
		ASSERT_JNLFILEID_NOT_NULL(csa);
		/* D9E04-002445 MUPIP RECOVER always open journal file without O_SYNC, ignoring jnl_sync_io */
		if (csd->jnl_sync_io && !mupip_jnl_recover)
		{
			OPENFILE_SYNC_CLOEXEC((sm_c_ptr_t)nameptr, O_RDWR, jpc->channel);
			jpc->sync_io = TRUE;
		} else
		{
			OPENFILE_CLOEXEC((sm_c_ptr_t)nameptr, O_RDWR, jpc->channel);
			jpc->sync_io = FALSE;
		}
		/* Check that if ever open errors out (i.e. return status is FD_INVALID=-1,
		 * jpc->channel will be already set to NOJNL (which is also defined to be -1).
		 */
		assert(FD_INVALID == NOJNL);
		if (FD_INVALID == jpc->channel)
		{
			jpc->status = errno;
			sts = ERR_JNLFILOPN;
		} else
		{
			FSTAT_FILE(jpc->channel, &stat_buf, fstat_res);
			ZOS_ONLY(gtm_zos_tag_to_policy(jpc->channel, TAG_BINARY, &realfiletag);)
		}
	}   /* if init */
	if (0 == sts)
	{
		if (0 == fstat_res)
		{
			if (init || is_gdid_stat_identical(&csa->nl->jnl_file.u, &stat_buf))
			{
				if (jnl_closed == csd->jnl_state)
				{	/* Operator close came in while opening */
					JNL_FD_CLOSE(jpc->channel, close_res);	/* sets jpc->channel to NOJNL */
					jpc->fileid.inode = 0;
					jpc->fileid.device = 0;
					jpc->pini_addr = 0;
				} else
				{
					if (init)
					{	/* Stash the file id in shared-memory for subsequent users */
						set_gdid_from_stat(&csa->nl->jnl_file.u, &stat_buf);
					}
					GTM_WHITE_BOX_TEST(WBTEST_JNL_FILE_OPEN_FAIL, sts, ERR_JNLFILOPN);
					DEBUG_ONLY(if (0 == sts))
						jpc->cycle = jb->cycle;	/* make private cycle and shared cycle in sync */
					GTM_FD_TRACE_ONLY(
						gtm_dbjnl_dupfd_check(); /* Check if db or jnl fds collide (D9I11-002714) */
						/* The dupfd check above should not reset our channel. */
						assertpro(NOJNL != jpc->channel);
					)
					START_JNL_FILE_CLOSE_TIMER_IF_NEEDED;
				}  /* if jnl_state */
			} else
			{	/* not init and file moved */
				SET_JPC_ERR_STR(ERR_JNLOPNERR, ERR_JNLMOVED, buff);
				assert(gtm_white_box_test_case_enabled
					&& ((WBTEST_JNLOPNERR_EXPECTED == gtm_white_box_test_case_number)
						|| (WBTEST_JNL_CREATE_FAIL == gtm_white_box_test_case_number)));
			}
		} else
		{	/* stat failed */
			SET_JPC_ERR_STR(ERR_JNLFILOPN, errno, buff);
		}
	}  /* sts == 0 when we started */
	if (0 != sts)
	{
		if (NOJNL != jpc->channel)
			JNL_FD_CLOSE(jpc->channel, close_res);	/* sets jpc->channel to NOJNL */
		assert(NOJNL == jpc->channel);
		jnl_send_oper(jpc, sts);
	}
	jpc->err_str = NULL;
	assert((0 != sts) || (NOJNL != jpc->channel));
	return sts;
}
