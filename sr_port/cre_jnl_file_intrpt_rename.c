/****************************************************************
 *								*
 * Copyright (c) 2003-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
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
GBLREF	boolean_t	is_src_server;

error_def(ERR_FILEPARSE);
error_def(ERR_RENAMEFAIL);
error_def(ERR_FILEDELFAIL);
error_def(ERR_FILEDEL);
error_def(ERR_FILERENAME);

/* Creation of a new journal file (i.e. journal file switch) proceeds in the following steps
 *	STEP1) mumps.mjl is current generation journal file
 *	STEP2) When mumps.mjl becomes full, the first step is to create mumps.mjl_%YGTM
 *	STEP3) Fill mumps.mjl_%YGTM journal file header with valid data
 *	STEP4) Rename mumps.mjl to mumps.mjl_<timestamp> (e.g. mumps.mjl_2018144122944)
 *	STEP5) Rename mumps.mjl_%YGTM to mumps.mjl
 * The below function cleans up or finishes any remaining steps in case a process doing the above gets killed prematurely.
 */

/* Returns 0 on success and non-zero otherwise (e.g. ERR_FILEDELFAIL etc.) */
uint4	cre_jnl_file_intrpt_rename(sgmnt_addrs *csa)
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
		return ERR_FILEPARSE;
	}
	filestr.addr = (char *)ext_new_jnl_fn;
	filestr.len = ext_new_jnl_fn_len;
	status2 = gtm_file_stat(&filestr, NULL, NULL, FALSE, &ustatus);
	if (FILE_STAT_ERROR == status2)
	{
		send_msg_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_FILEPARSE, 2, filestr.len, filestr.addr, ustatus);
		if (!(IS_GTM_IMAGE))
			gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_FILEPARSE, 2, filestr.len, filestr.addr, ustatus);
		return ERR_FILEPARSE;
	}
	/* If we are the source server, try to do the rename or file delete but do not issue RENAMEFAIL or FILEDELFAIL errors
	 * in case of failures. This is because, all the source server needs is to open a journal file to read journal records
	 * and it can do so without doing the rename or file delete. Returning ERR_RENAMEFAIL or ERR_FILEDELFAIL to caller
	 * will take care of appropriate action in "repl_ctl_create".
	 */
	if (FILE_NOT_FOUND == status1)
	{	/* mumps.mjl is NOT present. This means a process in journal file switch had finished STEP4 but got killed
		 * before it could finish STEP5.
		 */
		if (FILE_PRESENT == status2)
		{
			status = gtm_rename(filestr.addr, (int)filestr.len, (char *)fn, fn_len, &ustatus);
			if (SYSCALL_ERROR(status))
			{
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(7) ERR_RENAMEFAIL, 4, filestr.len, filestr.addr,
						fn_len, fn, status);
				if (!(IS_GTM_IMAGE) && !is_src_server)
					gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(7) ERR_RENAMEFAIL, 4, filestr.len, filestr.addr,
							fn_len, fn, status);
				return ERR_RENAMEFAIL;
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
	{	/* mumps.mjl is present. This means STEP4 did not yet execute. */
		if (FILE_PRESENT == status2)
		{	/* mumps.mjl_%YGTM is present. This means STEP2 is done. But since STEP4 is not yet done
			 * we do not know if STEP3 (filling jnl file header with valid data) happened completely so
			 * just discard this file. It will be re-created by the next process attempting the switch.
			 */
			status = gtm_file_remove(filestr.addr, (int)filestr.len, &ustatus);
			if (SYSCALL_ERROR(status))
			{
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_FILEDELFAIL, 2, filestr.len, filestr.addr, status);
				if (!(IS_GTM_IMAGE) && !is_src_server)
					gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_FILEDELFAIL, 2, filestr.len,
								filestr.addr, status);
				return ERR_FILEDELFAIL;
			} else
			{
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_FILEDEL, 2, filestr.len, filestr.addr);
				if (!(IS_GTM_IMAGE))
					gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_FILEDEL, 2, filestr.len, filestr.addr);
			}
		}
	}
	return 0;
}
