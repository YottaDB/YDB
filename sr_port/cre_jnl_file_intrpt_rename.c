/****************************************************************
 *								*
 *	Copyright 2003, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_rename.h"
#include "gtm_file_remove.h"
#include "gtm_file_stat.h"
#include "gtmmsg.h"
#include "iosp.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "send_msg.h"
#include "gtmimagename.h"

void cre_jnl_file_intrpt_rename(int fn_len, sm_uc_ptr_t fn)
{
	mstr 		filestr;
	int		status1, status2, ext_new_jnl_fn_len;
	uint4		status, ustatus;
	unsigned char	ext_new_jnl_fn[MAX_FN_LEN];
	error_def(ERR_FILEPARSE);
	error_def(ERR_RENAMEFAIL);
	error_def(ERR_FILEDELFAIL);
	error_def(ERR_FILEDEL);
	error_def(ERR_FILERENAME);

	filestr.addr = (char *)fn;
	filestr.len = fn_len;
	prepare_unique_name((char *)fn, fn_len, "", EXT_NEW, (char *)ext_new_jnl_fn, &ext_new_jnl_fn_len, 0, &ustatus);
	assert(SS_NORMAL == ustatus);
	status1 = gtm_file_stat(&filestr, NULL, NULL, FALSE, &ustatus);
	if (FILE_STAT_ERROR == status1)
	{
		if (IS_GTM_IMAGE)
			send_msg(VARLSTCNT(5) ERR_FILEPARSE, 2, filestr.len, filestr.addr, ustatus);
		else
			gtm_putmsg(VARLSTCNT(5) ERR_FILEPARSE, 2, filestr.len, filestr.addr, ustatus);
		return;
	}
	filestr.addr = (char *)ext_new_jnl_fn;
	filestr.len = ext_new_jnl_fn_len;
	status2 = gtm_file_stat(&filestr, NULL, NULL, FALSE, &ustatus);
	if (FILE_STAT_ERROR == status2)
	{
		if (IS_GTM_IMAGE)
			send_msg(VARLSTCNT(5) ERR_FILEPARSE, 2, filestr.len, filestr.addr, ustatus);
		else
			gtm_putmsg(VARLSTCNT(5) ERR_FILEPARSE, 2, filestr.len, filestr.addr, ustatus);
		return;
	}
	if (FILE_NOT_FOUND == status1)
	{
		if (FILE_PRESENT == status2)
		{
			status = gtm_rename(filestr.addr, (int)filestr.len, (char *)fn, fn_len, &ustatus);
			if (SYSCALL_ERROR(status))
			{
				if (IS_GTM_IMAGE)
				{
					VMS_ONLY(send_msg(VARLSTCNT(8) ERR_RENAMEFAIL, 4, filestr.len, filestr.addr,
						fn_len, fn, status, ustatus);)
					UNIX_ONLY(send_msg(VARLSTCNT(7) ERR_RENAMEFAIL, 4, filestr.len, filestr.addr,
						fn_len, fn, status);)
				} else
				{
					VMS_ONLY(gtm_putmsg(VARLSTCNT(8) ERR_RENAMEFAIL, 4, filestr.len, filestr.addr,
						fn_len, fn, status, ustatus);)
					UNIX_ONLY(gtm_putmsg(VARLSTCNT(7) ERR_RENAMEFAIL, 4, filestr.len, filestr.addr,
						fn_len, fn, status);)
				}
			} else
			{
				if (IS_GTM_IMAGE)
					send_msg(VARLSTCNT(6) ERR_FILERENAME, 4, (int)filestr.len, filestr.addr, fn_len, fn);
				else
					gtm_putmsg(VARLSTCNT(6) ERR_FILERENAME, 4, filestr.len, filestr.addr, fn_len, fn);
			}
		}
	} else
	{
		if (FILE_PRESENT == status2)
		{
			status = gtm_file_remove(filestr.addr, (int)filestr.len, &ustatus);
			if (SYSCALL_ERROR(status))
			{
				if (IS_GTM_IMAGE)
				{
					VMS_ONLY(send_msg(VARLSTCNT(6) ERR_FILEDELFAIL, 2, filestr.len, filestr.addr,
								status, ustatus);)
					UNIX_ONLY(send_msg(VARLSTCNT(5) ERR_FILEDELFAIL, 2, filestr.len, filestr.addr, status);)
				} else
				{
					VMS_ONLY(gtm_putmsg(VARLSTCNT(6) ERR_FILEDELFAIL, 2, filestr.len, filestr.addr,
								status, ustatus);)
					UNIX_ONLY(gtm_putmsg(VARLSTCNT(5) ERR_FILEDELFAIL, 2, filestr.len, filestr.addr, status);)
				}
			} else
			{
				if (IS_GTM_IMAGE)
					send_msg(VARLSTCNT(4) ERR_FILEDEL, 2, filestr.len, filestr.addr);
				else
					gtm_putmsg(VARLSTCNT(4) ERR_FILEDEL, 2, filestr.len, filestr.addr);
			}
		}
	}
}
