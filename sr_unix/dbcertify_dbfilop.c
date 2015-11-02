/****************************************************************
 *								*
 *	Copyright 2005, 2012 Fidelity Information Services, Inc *
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stdio.h"
#include "gtm_fcntl.h"
#include "gtm_limits.h"
#include "gtm_stdlib.h"
#include "gtm_unistd.h"
#include <sys/sem.h>
#include <errno.h>

#include "gtmio.h"
#include "gdsroot.h"
#include "v15_gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "v15_gdsbt.h"
#include "gdsfhead.h"
#include "v15_gdsfhead.h"
#include "filestruct.h"
#include "v15_filestruct.h"
#include "iosp.h"
#include "eintr_wrappers.h"
#include "gdsblk.h"
#include "gdsblkops.h"
#include "is_file_identical.h"
#include "error.h"
#include "dbcertify.h"
#include "jnl.h"
#include "anticipatory_freeze.h"

error_def(ERR_DBFILOPERR);
error_def(ERR_DBOPNERR);
error_def(ERR_DBPREMATEOF);
error_def(ERR_SYSCALL);
error_def(ERR_TEXT);

void dbcertify_dbfilop(phase_static_area *psa)
{
	unix_db_info	*udi;
	struct stat	stat_buf;
	int4		save_errno;
	int		fstat_res;

	/* assert((dba_mm == psa->fc->file_type) || (dba_bg == psa->fc->file_type)); not always set in unix */
	udi = (unix_db_info *)psa->fc->file_info;
	switch(psa->fc->op)
	{
		case FC_READ:
			NON_GTM64_ONLY(DBC_DEBUG(("DBC_DEBUG: -- Reading database op_pos = %lld  op_len = %d\n",
					     psa->fc->op_pos, psa->fc->op_len)));
			GTM64_ONLY(DBC_DEBUG(("DBC_DEBUG: -- Reading database op_pos = %ld  op_len = %d\n",
						 psa->fc->op_pos, psa->fc->op_len)));
			assert(psa->fc->op_pos > 0);		/* gt.m uses the vms convention of numbering the blocks from 1 */
			LSEEKREAD(udi->fd,
				  (off_t)(psa->fc->op_pos - 1) * DISK_BLOCK_SIZE,
				  psa->fc->op_buff,
				  psa->fc->op_len,
				  save_errno);
			if (0 != save_errno)
			{
				if (-1 == save_errno)
					rts_error(VARLSTCNT(4) ERR_DBPREMATEOF, 2, LEN_AND_STR(udi->fn));
				else
					rts_error(VARLSTCNT(5) ERR_DBFILOPERR, 2, LEN_AND_STR(udi->fn), save_errno);
			}
			break;
		case FC_WRITE:
			NON_GTM64_ONLY(DBC_DEBUG(("DBC_DEBUG: -- Writing database op_pos = %lld  op_len = %d\n",
					     psa->fc->op_pos, psa->fc->op_len)));
			GTM64_ONLY(DBC_DEBUG(("DBC_DEBUG: -- Writing database op_pos = %ld  op_len = %d\n",
						 psa->fc->op_pos, psa->fc->op_len)));
			DB_LSEEKWRITE(NULL, NULL, udi->fd,
				   (off_t)(psa->fc->op_pos - 1) * DISK_BLOCK_SIZE,
				   psa->fc->op_buff,
				   psa->fc->op_len,
				   save_errno);
			if (0 != save_errno)
				rts_error(VARLSTCNT(5) ERR_DBFILOPERR, 2, LEN_AND_STR(udi->fn), save_errno);
			break;
		case FC_OPEN:
			DBC_DEBUG(("DBC_DEBUG: -- Opening database %s\n", (char *)psa->dbc_gv_cur_region->dyn.addr->fname));
			psa->dbc_gv_cur_region->read_only = FALSE;	/* maintain csa->read_write simultaneously */
			udi->s_addrs.read_write = TRUE;			/* maintain reg->read_only simultaneously */
			if (FD_INVALID == (udi->fd = OPEN((char *)psa->dbc_gv_cur_region->dyn.addr->fname, O_RDWR)))
			{
				if (FD_INVALID == (udi->fd = OPEN((char *)psa->dbc_gv_cur_region->dyn.addr->fname, O_RDONLY)))
				{
					save_errno = errno;
					rts_error(VARLSTCNT(5) ERR_DBOPNERR, 2, DB_LEN_STR(psa->dbc_gv_cur_region), save_errno);
				}
				psa->dbc_gv_cur_region->read_only = TRUE;	/* maintain csa->read_write simultaneously */
				udi->s_addrs.read_write = FALSE;		/* maintain reg->read_only simultaneously */
			}
			FSTAT_FILE(udi->fd, &stat_buf, fstat_res);
			if (-1 == fstat_res)
			{
				save_errno = errno;
				rts_error(VARLSTCNT(5) ERR_DBOPNERR, 2, DB_LEN_STR(psa->dbc_gv_cur_region), save_errno);
			}
			set_gdid_from_stat(&udi->fileid, &stat_buf);
			udi->raw = (S_ISCHR(stat_buf.st_mode) || S_ISBLK(stat_buf.st_mode));
			udi->fn = (char *)psa->dbc_gv_cur_region->dyn.addr->fname;
			break;
		case FC_CLOSE:
			DBC_DEBUG(("DBC_DEBUG: -- Closing database %s\n", (char *)psa->dbc_gv_cur_region->dyn.addr->fname));
			CLOSEFILE_RESET(udi->fd, save_errno);	/* resets "udi->fd" to FD_INVALID */
			if (0 != save_errno)
				rts_error(VARLSTCNT(5) ERR_DBFILOPERR, 2, LEN_AND_STR(udi->fn), save_errno);
			break;
		default:
			GTMASSERT;
	}
}
