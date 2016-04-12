/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_ipc.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_stat.h"
#include "gtm_stdio.h"
#include "gtm_string.h"

#ifdef __MVS__
#include "gtm_zos_io.h"
#endif
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
#include "mupip_set.h"
#include "mupint.h"
#include "eintr_wrappers.h"
#include "util.h"
#include "mu_rndwn_file.h"
#include "db_ipcs_reset.h"
#include "anticipatory_freeze.h"

GBLREF gd_region        *gv_cur_region;
GBLREF boolean_t	need_no_standalone;

error_def(ERR_BADTAG);	/* Need for TAG_POLICY macro */
error_def(ERR_JNLFILNOTCHG);
error_def(ERR_TEXT);	/* Need for TAG_POLICY macro */

int4 mupip_set_jnlfile(char *jnl_fname, int jnl_fn_len)
{
	int 		jnl_fd, status;
	char		hdr_buffer[REAL_JNL_HDR_LEN];
	jnl_file_header	*header;
	char		*errptr;
	int		save_no;
	int		rc;
	ZOS_ONLY(int 	realfiletag;)

	OPENFILE(jnl_fname, O_RDWR, jnl_fd);
	if (FD_INVALID == jnl_fd)
	{
		save_no = errno;
		util_out_print("Error opening journal file !AD", TRUE, LEN_AND_STR(jnl_fname));
		errptr = (char *)STRERROR(save_no);
		util_out_print("open : !AZ", TRUE, errptr);
		return((int4)ERR_JNLFILNOTCHG);
	}
#ifdef __MVS__
	if (-1 == gtm_zos_tag_to_policy(jnl_fd, TAG_BINARY, &realfiletag))
		TAG_POLICY_GTM_PUTMSG(jnl_fname, realfiletag, TAG_BINARY, errno);
#endif
	LSEEKREAD(jnl_fd, 0, (sm_uc_ptr_t)hdr_buffer, SIZEOF(hdr_buffer), status);
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
	if (SS_NORMAL != (status = mupip_set_jnlfile_aux(header, jnl_fname)))
		return status;
	JNL_LSEEKWRITE(NULL, NULL, jnl_fd, 0, (sm_uc_ptr_t)hdr_buffer, SIZEOF(hdr_buffer), status);
	if (0 != status)
	{
		save_no = errno;
		util_out_print("Error writing journal file !AD", TRUE, LEN_AND_STR(jnl_fname));
		errptr = (char *)STRERROR(save_no);
		util_out_print("write : !AZ", TRUE, errptr);
		return((int4)ERR_JNLFILNOTCHG);
	}
	CLOSEFILE_RESET(jnl_fd, rc);	/* resets "jnl_fd" to FD_INVALID */
	if (-1 == rc)
	{
		save_no = errno;
		util_out_print("Error closing journal file !AD", TRUE, LEN_AND_STR(jnl_fname));
		errptr = (char *)STRERROR(save_no);
		util_out_print("close :!AZ", TRUE, errptr);
		return((int4)ERR_JNLFILNOTCHG);
	}
	if (!need_no_standalone)
		db_ipcs_reset(gv_cur_region);
	return SS_NORMAL;
}
