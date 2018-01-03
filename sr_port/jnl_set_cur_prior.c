/****************************************************************
 *								*
 * Copyright (c) 2017 Fidelity National Information		*
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
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "gtmio.h"
#include "anticipatory_freeze.h"
#include "is_file_identical.h"

/* For use when disabling journaling in standalone to mark the current journal file as being a prior journal file.
 * The journal file is not always present/usable/writable, in which case we can't change it, so ignore errors.
 */
void jnl_set_cur_prior(gd_region *reg, sgmnt_addrs *csa, sgmnt_data *csd)
{
	int			jnl_fd;
	unix_db_info		*udi;

	udi = FILE_INFO(reg);
	assert('\0' == csd->jnl_file_name[csd->jnl_file_len]);
	assert(udi->grabbed_access_sem || (csa->now_crit && !csa->nl->jnl_file.u.inode));
	OPENFILE((char *)csd->jnl_file_name, O_RDWR, jnl_fd);
	if (FD_INVALID != jnl_fd)
		jnl_set_fd_prior(jnl_fd, csa, csd, NULL);
}

/* Same as above, but for when we already have the file open.
 * If jfh is non-NULL, it is assumed correct, updated, and written; otherwise, a fresh copy is read from the file.
 */
void jnl_set_fd_prior(int jnl_fd, sgmnt_addrs* csa, sgmnt_data* csd, jnl_file_header *jfh)
{
	int			status1, status2;
	jnl_file_header		header, *jfh_checked = NULL;

	if (NULL == jfh)
	{
		DO_FILE_READ(jnl_fd, 0, &header, SIZEOF(header), status1, status2);
		assert(SS_NORMAL == status1);
		if (SS_NORMAL == status1)
		{
			CHECK_JNL_FILE_IS_USABLE(&header, status1, FALSE, 0, NULL);
			assert(SS_NORMAL == status1);
			if ((SS_NORMAL == status1) && !header.is_not_latest_jnl)
				jfh_checked = &header;
		}
	} else
		jfh_checked = jfh;
	/* Only do an update if we successfully read the journal header and the database file in the journal header matches
	 * the current database file. A mismatch may occur when working with a database backup on the same machine as the
	 * original database, as the backup will still point to the original journal file.
	 */
	if ((NULL != jfh_checked) && is_file_identical((char *)csa->region->dyn.addr->fname, (char *)jfh_checked->data_file_name))
	{
		jfh_checked->is_not_latest_jnl = TRUE;
		JNL_DO_FILE_WRITE(csa, csd->jnl_file_name, jnl_fd, 0, jfh_checked, REAL_JNL_HDR_LEN, status1, status2);
		assert(SS_NORMAL == status1);
	}
	JNL_FD_CLOSE(jnl_fd, status1);
}
