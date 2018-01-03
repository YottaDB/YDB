/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
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
#include "gtm_fcntl.h"
#include "gtm_unistd.h"

#include <sys/sem.h>
#include <errno.h>
#include <stddef.h>

#ifdef __MVS__
#include "gtm_zos_io.h"
#endif
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
#include "anticipatory_freeze.h"
#include "jnl.h"
#include "get_fs_block_size.h"

GBLREF	gd_region	*gv_cur_region;

error_def(ERR_DBFILOPERR);
error_def(ERR_DBNOTGDS);
error_def(ERR_DBOPNERR);
error_def(ERR_DBPREMATEOF);
error_def(ERR_TEXT);

uint4 dbfilop(file_control *fc)
{
	unix_db_info		*udi;
	struct stat		stat_buf;
	int4			save_errno;
	int			fstat_res;
	off_t			offset;
	gd_segment		*seg;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	ZOS_ONLY(int		realfiletag;)

	udi = FC2UDI(fc);
	csa = &udi->s_addrs;
	switch(fc->op)
	{
		case FC_READ:
			assert(fc->op_pos > 0);		/* gt.m uses the vms convention of numbering the blocks from 1 */
			offset = (off_t)(fc->op_pos - 1) * DISK_BLOCK_SIZE;
			DB_LSEEKREAD(udi, udi->fd, offset, fc->op_buff, fc->op_len, save_errno);
			if (0 != save_errno)
			{
				if (-1 == save_errno)
					rts_error_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_DBPREMATEOF, 2, LEN_AND_STR(udi->fn));
				else
					rts_error_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_DBFILOPERR, 2, LEN_AND_STR(udi->fn),
														save_errno);
			}
			if (1 == fc->op_pos)
			{
				if (0 != memcmp(fc->op_buff, GDS_LABEL, GDS_LABEL_SZ - 3))
					rts_error_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_DBNOTGDS, 2, LEN_AND_STR(udi->fn));
				if (0 == memcmp(fc->op_buff, GDS_LABEL, GDS_LABEL_SZ - 1))	/* current GDS */
				{
					csd = (sgmnt_data_ptr_t)fc->op_buff;
					if (offsetof(sgmnt_data, minor_dbver) < fc->op_len)
						CHECK_DB_ENDIAN((sgmnt_data_ptr_t)fc->op_buff, strlen(udi->fn),
													udi->fn); /* BYPASSOK */
				}
			}
			break;
		case FC_WRITE:
			assertpro((1 != fc->op_pos) || ((0 == memcmp(fc->op_buff, GDS_LABEL, GDS_LABEL_SZ - 1))
					&& (0 != ((sgmnt_data_ptr_t)fc->op_buff)->acc_meth)));
			assert((1 != fc->op_pos) || (fc->op_len <= SIZEOF_FILE_HDR(fc->op_buff)));
			assert(!gv_cur_region->read_only);
			offset = (off_t)(fc->op_pos - 1) * DISK_BLOCK_SIZE;
			DB_LSEEKWRITE(csa, udi, udi->fn, udi->fd, offset, fc->op_buff, fc->op_len, save_errno);
			if (0 != save_errno)
				rts_error_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_DBFILOPERR, 2, LEN_AND_STR(udi->fn), save_errno);
			break;
		case FC_OPEN:
			gv_cur_region->read_only = FALSE;	/* maintain csa->read_write simultaneously */
			seg = gv_cur_region->dyn.addr;
			csa->read_write = TRUE;		/* maintain reg->read_only simultaneously */
			csa->orig_read_write = TRUE;	/* maintain orig_read_write at same time as read_write */
			OPENFILE_DB((char *)seg->fname, O_RDWR, udi, seg);
			if (FD_INVALID == udi->fd)
			{
				OPENFILE_DB((char *)seg->fname, O_RDONLY, udi, seg);
				if (FD_INVALID == udi->fd)
					return ERR_DBOPNERR;
				gv_cur_region->read_only = TRUE;	/* maintain csa->read_write simultaneously */
				csa->read_write = FALSE;	/* maintain reg->read_only simultaneously */
				csa->orig_read_write = FALSE;	/* maintain orig_read_write at same time as read_write */
			}
			FSTAT_FILE(udi->fd, &stat_buf, fstat_res);
			if (-1 == fstat_res)
				return ERR_DBOPNERR;
#			ifdef __MVS__
			if (-1 == gtm_zos_tag_to_policy(udi->fd, TAG_BINARY, &realfiletag))
				TAG_POLICY_SEND_MSG((char_ptr_t)gv_cur_region->dyn.addr->fname, errno, realfiletag, TAG_BINARY);
#			endif
			set_gdid_from_stat(&udi->fileid, &stat_buf);
			udi->raw = (S_ISCHR(stat_buf.st_mode) || S_ISBLK(stat_buf.st_mode));
			udi->fn = (char *)gv_cur_region->dyn.addr->fname;
			break;
		case FC_CLOSE:
			CLOSEFILE_RESET(udi->fd, save_errno);	/* resets "udi->fd" to FD_INVALID */
			if (0 != save_errno)
				rts_error_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_DBFILOPERR, 2, LEN_AND_STR(udi->fn), save_errno);
			udi->fd_opened_with_o_direct = FALSE;
			break;
		default:
			assertpro(FALSE && fc->op);
	}
	return SS_NORMAL;
}
