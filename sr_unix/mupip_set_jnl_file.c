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

#include <sys/ipc.h>
#include "gtm_fcntl.h"
#include <unistd.h>
#include "gtm_stat.h"
#include "gtm_stdio.h"
#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "gtmio.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "cli.h"
#include "iosp.h"
#include "jnl.h"
#include "mupipset.h"
#include "mupint.h"
#include "eintr_wrappers.h"
#include "util.h"
#include "mu_rndwn_file.h"
#include "ftok_sems.h"

GBLREF gd_region        *gv_cur_region;
GBLREF boolean_t	need_no_standalone;

int4 mupip_set_jnl_file(char *jnl_fname)
{
	int 		jnl_fd, status;
	char		hdr_buffer[sizeof(jnl_file_header)];
	jnl_file_header	*header;
	char		*errptr;
	int		save_no;

	error_def(ERR_JNLFILNOTCHG);


	OPENFILE(jnl_fname, O_RDWR, jnl_fd);
	if (-1 == jnl_fd)
	{
		save_no = errno;
		util_out_print("Error opening journal file !AD", TRUE, LEN_AND_STR(jnl_fname));
		errptr = (char *)STRERROR(save_no);
		util_out_print("open : !AZ", TRUE, errptr);
		return((int4)ERR_JNLFILNOTCHG);
	}

	LSEEKREAD(jnl_fd, 0, (sm_uc_ptr_t)hdr_buffer, sizeof(hdr_buffer), status);

	if (0 != status)
	{
		if (-1 != status)
		{
			save_no = errno;
			util_out_print("Error reading journal file !AD", TRUE, LEN_AND_STR(jnl_fname));
			errptr = (char *)STRERROR(save_no);
			util_out_print("read : !AZ", TRUE, errptr);
		}
		else
			util_out_print("Premature end of journal file !AD", TRUE, LEN_AND_STR(jnl_fname));
		return((int4)ERR_JNLFILNOTCHG);
	}

	header = (jnl_file_header *)hdr_buffer;
	if(SS_NORMAL != (status = mupip_set_jnlfile_aux(header)))
		return status;

	LSEEKWRITE(jnl_fd, 0, (sm_uc_ptr_t)hdr_buffer, sizeof(hdr_buffer), status);
	if (0 != status)
	{
		save_no = errno;
		util_out_print("Error writing journal file !AD", TRUE, LEN_AND_STR(jnl_fname));
		errptr = (char *)STRERROR(save_no);
		util_out_print("write : !AZ", TRUE, errptr);
		return((int4)ERR_JNLFILNOTCHG);
	}
	if (-1 == close(jnl_fd))
	{
		save_no = errno;
		util_out_print("Error closing journal file !AD", TRUE, LEN_AND_STR(jnl_fname));
		errptr = (char *)STRERROR(save_no);
		util_out_print("close :!AZ", TRUE, errptr);
		return((int4)ERR_JNLFILNOTCHG);
	}
	if (!need_no_standalone)
		db_ipcs_reset(gv_cur_region, FALSE);
	return SS_NORMAL;
}
