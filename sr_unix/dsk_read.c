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

#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "iosp.h"
#include "error.h"
#include "gtmio.h"

GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_addrs	*cs_addrs;

int4	dsk_read (block_id blk, sm_uc_ptr_t buff)
{
	unix_db_info	*udi;
	int4		size, save_errno;

	udi = (unix_db_info *)(gv_cur_region->dyn.addr->file_cntl->file_info);
	size = cs_addrs->hdr->blk_size;
	assert (cs_addrs->hdr->acc_meth == dba_bg);

	LSEEKREAD(udi->fd,
		  (DISK_BLOCK_SIZE * (cs_addrs->hdr->start_vbn - 1) + (off_t)blk * size),
		  buff,
		  size,
		  save_errno);


	return save_errno;
}
