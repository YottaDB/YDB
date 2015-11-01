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

#include <sys/sem.h>
#include "gtm_fcntl.h"
#include <unistd.h>
#include <errno.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "eintr_wrappers.h"
#include "gtmio.h"
#include "iosp.h"
#include "is_file_identical.h"
#include "dbfilop.h"

GBLREF	gd_region	*gv_cur_region;


uint4 dbfilop(file_control *fc)
{
	unix_db_info	*udi;
	struct stat	stat_buf;
	int4		save_errno;
	int		fstat_res;

	error_def(ERR_DBFILOPERR);
	error_def(ERR_DBNOTGDS);
	error_def(ERR_DBOPNERR);
	error_def(ERR_DBPREMATEOF);

	/* assert((dba_mm == fc->file_type) || (dba_bg == fc->file_type)); not always set in unix */
	udi = (unix_db_info *)fc->file_info;
	switch(fc->op)
	{
		case FC_READ:
				assert(fc->op_pos > 0);		/* gt.m uses the vms convention of numbering the blocks from 1 */
 				LSEEKREAD(udi->fd,
 					  (off_t)(fc->op_pos - 1) * DISK_BLOCK_SIZE,
 					  fc->op_buff,
 					  fc->op_len,
 					  save_errno);
				if (0 != save_errno)
				{
					if (-1 == save_errno)
						rts_error(VARLSTCNT(4) ERR_DBPREMATEOF, 2, LEN_AND_STR(udi->fn));
					else
						rts_error(VARLSTCNT(5) ERR_DBFILOPERR, 2, LEN_AND_STR(udi->fn), save_errno);
				}
				if ((1 == fc->op_pos) && (0 != memcmp(fc->op_buff, GDS_LABEL, GDS_LABEL_SZ - 3)))
					rts_error(VARLSTCNT(4) ERR_DBNOTGDS, 2, LEN_AND_STR(udi->fn));
				break;
		case FC_WRITE:
				if ((1 == fc->op_pos) && ((0 != memcmp(fc->op_buff, GDS_LABEL, GDS_LABEL_SZ - 1))
						|| (0 == ((sgmnt_data_ptr_t)fc->op_buff)->acc_meth)))
					GTMASSERT;
 				LSEEKWRITE(udi->fd,
 					   (off_t)(fc->op_pos - 1) * DISK_BLOCK_SIZE,
 					   fc->op_buff,
 					   fc->op_len,
 					   save_errno);
 				if (0 != save_errno)
					rts_error(VARLSTCNT(5) ERR_DBFILOPERR, 2, LEN_AND_STR(udi->fn), save_errno);
				break;
		case FC_OPEN:
				if (-1 == (udi->fd = OPEN((char *)gv_cur_region->dyn.addr->fname, O_RDWR)))
				{
					if (-1 == (udi->fd = OPEN((char *)gv_cur_region->dyn.addr->fname, O_RDONLY)))
						return ERR_DBOPNERR;
					gv_cur_region->read_only = TRUE;
				}
				FSTAT_FILE(udi->fd, &stat_buf, fstat_res);
				if (-1 == fstat_res)
					return ERR_DBOPNERR;
				set_gdid_from_stat(&udi->fileid, &stat_buf);
				udi->raw = (S_ISCHR(stat_buf.st_mode) || S_ISBLK(stat_buf.st_mode));
				udi->fn = (char *)gv_cur_region->dyn.addr->fname;
				break;
		case FC_CLOSE:
				CLOSEFILE(udi->fd, save_errno);
 				if (0 != save_errno)
					rts_error(VARLSTCNT(5) ERR_DBFILOPERR, 2, LEN_AND_STR(udi->fn), save_errno);
				break;
		default:
				GTMASSERT;
	}
	return SS_NORMAL;
}
