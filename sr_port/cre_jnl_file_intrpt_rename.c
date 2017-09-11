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

GBLREF	gd_region	*gv_cur_region;

error_def(ERR_FILEPARSE);
error_def(ERR_RENAMEFAIL);
error_def(ERR_FILEDELFAIL);
error_def(ERR_FILEDEL);
error_def(ERR_FILERENAME);

void cre_jnl_file_intrpt_rename(sgmnt_addrs *csa)
{
	int		fn_len;
	sm_uc_ptr_t	fn;
	mstr 		filestr;
	int		status1, status2, ext_new_jnl_fn_len;
	uint4		status, ustatus;
	unsigned char	ext_new_jnl_fn[MAX_FN_LEN + STR_LIT_LEN(EXT_NEW) + 1];

	assert(csa);
	assert(csa->hdr);
	/* We need either crit or standalone to ensure that there are no concurrent switch attempts. */
	assert(csa->now_crit || (gv_cur_region && FILE_INFO(gv_cur_region)->grabbed_access_sem));
	if (!csa->hdr)
		return;
	fn = csa->hdr->jnl_file_name;
	fn_len = csa->hdr->jnl_file_len;
	filestr.addr = (char *)fn;
	filestr.len = fn_len;
	ext_new_jnl_fn_len = ARRAYSIZE(ext_new_jnl_fn);
	status = prepare_unique_name((char *)fn, fn_len, "", EXT_NEW, (char *)ext_new_jnl_fn, &ext_new_jnl_fn_len, 0, &ustatus);
	/* We have allocated enough space in ext_new_jnl_fn array to store EXT_NEW suffix.
	 * So no way the above "prepare_unique_name" call can fail. Hence the below assert.
	 */
	assert(SS_NORMAL == status);
	status1 = gtm_file_stat(&filestr, NULL, NULL, FALSE, &ustatus);
	if (FILE_STAT_ERROR == status1)
	{
		send_msg_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_FILEPARSE, 2, filestr.len, filestr.addr, ustatus);
		if (!(IS_GTM_IMAGE))
			gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_FILEPARSE, 2, filestr.len, filestr.addr, ustatus);
		return;
	}
	filestr.addr = (char *)ext_new_jnl_fn;
	filestr.len = ext_new_jnl_fn_len;
	status2 = gtm_file_stat(&filestr, NULL, NULL, FALSE, &ustatus);
	if (FILE_STAT_ERROR == status2)
	{
		send_msg_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_FILEPARSE, 2, filestr.len, filestr.addr, ustatus);
		if (!(IS_GTM_IMAGE))
			gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_FILEPARSE, 2, filestr.len, filestr.addr, ustatus);
		return;
	}
	if (FILE_NOT_FOUND == status1)
	{
		if (FILE_PRESENT == status2)
		{
			status = gtm_rename(filestr.addr, (int)filestr.len, (char *)fn, fn_len, &ustatus);
			if (SYSCALL_ERROR(status))
			{
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(7) ERR_RENAMEFAIL, 4, filestr.len, filestr.addr,
						fn_len, fn, status);
				if (!(IS_GTM_IMAGE))
					gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(7) ERR_RENAMEFAIL, 4, filestr.len, filestr.addr,
							fn_len, fn, status);
			} else
			{
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_FILERENAME, 4, (int)filestr.len, filestr.addr,
						fn_len, fn);
				if (!(IS_GTM_IMAGE))
					gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_FILERENAME, 4, filestr.len, filestr.addr,
							fn_len, fn);
			}
		}
	} else
	{
		if (FILE_PRESENT == status2)
		{
			status = gtm_file_remove(filestr.addr, (int)filestr.len, &ustatus);
			if (SYSCALL_ERROR(status))
			{
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_FILEDELFAIL, 2, filestr.len, filestr.addr, status);
				if (!(IS_GTM_IMAGE))
					gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_FILEDELFAIL, 2, filestr.len,
								filestr.addr, status);
			} else
			{
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_FILEDEL, 2, filestr.len, filestr.addr);
				if (!(IS_GTM_IMAGE))
					gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_FILEDEL, 2, filestr.len, filestr.addr);
			}
		}
	}
}
