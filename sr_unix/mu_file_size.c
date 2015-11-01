/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gtm_stat.h"
#include "eintr_wrappers.h"
#include "mu_file_size.h"

unsigned int mu_file_size(file_control *fc)
{
	unix_db_info	*udi;
	int		fstat_res;
	struct stat	stat_buf;

	error_def(ERR_DBFILOPERR);

	udi = (unix_db_info *)fc->file_info;
	FSTAT_FILE(udi->fd, &stat_buf, fstat_res);
	if (-1 == fstat_res)
		rts_error(VARLSTCNT(5) ERR_DBFILOPERR, 2, LEN_AND_STR(udi->fn), errno);
	return stat_buf.st_size / DISK_BLOCK_SIZE;
}
