/****************************************************************
 *								*
 *	Copyright 2001, 2005 Fidelity Information Services, Inc	*
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
#include "interlock.h"
#include "lockconst.h"
#include "aswp.h"
#include "is_file_identical.h"

error_def(ERR_JNLFILOPN);
error_def(ERR_JNLMOVED);
error_def(ERR_JNLOPNERR);
error_def(ERR_JNLRDERR);

uint4 jnl_file_open(gd_region *reg, bool init, int4 dummy)	/* third argument for compatibility with VMS version */
{
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	jnl_private_control	*jpc;
	jnl_buffer_ptr_t     	jb;
	struct stat		stat_buf;
	uint4			sts;
	sm_uc_ptr_t		nameptr;
	int			fstat_res;
	int			stat_res;
	boolean_t		retry;

	csa = &FILE_INFO(reg)->s_addrs;
	csd = csa->hdr;
	jpc = csa->jnl;
	jb = jpc->jnl_buff;
	assert(NOJNL == jpc->channel);
	sts = 0;
	jpc->status = jpc->status2 = SS_NORMAL;
	nameptr = csd->jnl_file_name;
	assert('/' == csd->jnl_file_name[0]);
	if (init)
	{
		assert(csd->jnl_file_len < JNL_NAME_SIZE);
		nameptr[csd->jnl_file_len] = 0;
		cre_jnl_file_intrpt_rename(((int)csd->jnl_file_len), csd->jnl_file_name);
		/* although jnl_file_close() would have reset jnl_file.u.inode and device to 0 and incremented cycle, it
		 * might have got shot in the middle of executing those instructions. we redo it here just to be safe.
		 */
		csa->nl->jnl_file.u.inode = 0;
		csa->nl->jnl_file.u.device = 0;
		jb->cycle++;
		for (retry = TRUE;  ;)
		{
			if (csd->jnl_sync_io)
			{
				OPENFILE_SYNC((sm_c_ptr_t)csd->jnl_file_name, O_RDWR, jpc->channel);
				jpc->sync_io = TRUE;
			} else
			{
				OPENFILE((sm_c_ptr_t)csd->jnl_file_name, O_RDWR, jpc->channel);
				jpc->sync_io = FALSE;
			}
			if (-1 == jpc->channel)
			{
				jpc->status = errno;
				sts = ERR_JNLFILOPN;
				jpc->channel = NOJNL;
				if (EACCES == jpc->status || EROFS == jpc->status)
					break;
			} else
			{
				FSTAT_FILE(jpc->channel, &stat_buf, fstat_res);
				if (-1 == fstat_res)
				{
					jpc->status = errno;
					sts = ERR_JNLRDERR;
					close(jpc->channel);
					jpc->channel = NOJNL;
					break;
				} else
					sts = jnl_file_open_common(reg, (off_jnl_t) stat_buf.st_size);
			}
			if (0 != sts && (retry))
			{
				sts = jnl_file_open_switch(reg, sts);
				retry = FALSE;	/* Do not switch more than once, even if error occurs */
				if (0 == sts)
					continue;
			}
			break;
		}
	} else   /* not init */
	{
		assert(0 == nameptr[csd->jnl_file_len]);
		ASSERT_JNLFILEID_NOT_NULL(csa);
		if (csd->jnl_sync_io)
		{
			OPENFILE_SYNC((sm_c_ptr_t)nameptr, O_RDWR, jpc->channel);
			jpc->sync_io = TRUE;
		} else
		{
			OPENFILE((sm_c_ptr_t)nameptr, O_RDWR, jpc->channel);
			jpc->sync_io = FALSE;
		}
		if (-1 == jpc->channel)
		{
			jpc->status = errno;
			sts = ERR_JNLFILOPN;
			jpc->channel = NOJNL;
		}
	}   /* if init */
	if (0 == sts)
	{
		STAT_FILE((sm_c_ptr_t)nameptr, &stat_buf, stat_res);
		if (0 == stat_res)
		{
			if (init || is_gdid_stat_identical(&csa->nl->jnl_file.u, &stat_buf))
			{
				if (jnl_closed == csd->jnl_state)
				{	/* Operator close came in while opening */
					close(jpc->channel);
					jpc->channel = NOJNL;
					jpc->fileid.inode = 0;
					jpc->fileid.device = 0;
					jpc->pini_addr = 0;
				} else
				{
					if (init)
					{	/* Stash the file id in shared-memory for subsequent users */
						set_gdid_from_stat(&csa->nl->jnl_file.u, &stat_buf);
					}
					jpc->cycle = jb->cycle;	/* make private cycle and shared cycle in sync */
				}  /* if jnl_state */
			} else
			{  /* not init and file moved */
				jpc->status = ERR_JNLMOVED;
				sts = ERR_JNLOPNERR;
			}
		} else
		{	/* stat failed */
			jpc->status = errno;
			sts = ERR_JNLFILOPN;
		}
	}  /* sts == 0 when we started */
	if (0 != sts)
	{
		if (NOJNL != jpc->channel)
			close(jpc->channel);
		jpc->channel = NOJNL;
		jnl_send_oper(jpc, sts);
	}
	return sts;
}
