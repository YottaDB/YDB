/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_unistd.h"
#include <errno.h>

#include "gtm_stdio.h"
#include "io.h"
#include "trans_log_name.h"
#include "gtm_string.h"
#include "gtcm_open_cmerrlog.h"
#include "iosp.h"
#include "gtm_rename.h"
#include "gtm_logicals.h"
#ifdef __MVS__
#include "gtm_zos_io.h"
#endif

#define GTCM_GNP_CMERR_FN		GTM_LOG_ENV "/gtcm_gnp_server.log"

GBLREF bool	gtcm_errfile;
GBLREF bool	gtcm_firsterr;
GBLREF FILE	*gtcm_errfs;
GBLREF char	gtcm_gnp_server_log[];
GBLREF int	gtcm_gnp_log_path_len;

void gtcm_open_cmerrlog(void)
{
	int		len;
	mstr		lfn1, lfn2;
	char		lfn_path[MAX_TRANS_NAME_LEN + 1];
	char		new_lfn_path[MAX_TRANS_NAME_LEN + 1];
	int		new_len;
	uint4 		ustatus;
	int4 		rval;
	FILE 		*new_file;

	error_def(ERR_TEXT);
	ZOS_ONLY(error_def(ERR_BADTAG);)

	if (0 != (len = STRLEN(gtcm_gnp_server_log)))
	{
		lfn1.addr = gtcm_gnp_server_log;
		lfn1.len = len;
	} else
	{
		lfn1.addr = GTCM_GNP_CMERR_FN;
		lfn1.len = SIZEOF(GTCM_GNP_CMERR_FN) - 1;
	}
	rval = TRANS_LOG_NAME(&lfn1, &lfn2, lfn_path, SIZEOF(lfn_path), do_sendmsg_on_log2long);
	if (rval == SS_NORMAL || rval == SS_NOLOGNAM)
	{
		lfn_path[lfn2.len] = 0;
		rename_file_if_exists(lfn_path, lfn2.len, new_lfn_path, &new_len, &ustatus);
#ifdef __MVS__
		if (-1 == gtm_zos_create_tagged_file(lfn_path, TAG_EBCDIC))
			TAG_POLICY_GTM_PUTMSG(lfn_path, errno, -1, TAG_EBCDIC);
#endif
		new_file = Fopen(lfn_path, "a");
		if (NULL != new_file)
		{
			gtcm_errfile = TRUE;
			if (gtcm_errfs)
				fclose(gtcm_errfs);
			gtcm_errfs = new_file;
			if (dup2(fileno(gtcm_errfs), 1) < 0)
			{
				rts_error(VARLSTCNT(5) ERR_TEXT, 2, LEN_AND_LIT("Error on dup2 of stdout"), errno);
			}
			if (dup2(fileno(gtcm_errfs), 2) < 0)
			{
				rts_error(VARLSTCNT(5) ERR_TEXT, 2, LEN_AND_LIT("Error on dup2 of stderr"), errno);
			}
		}
		else
			fprintf(stderr, "Unable to open %s : %s\n", lfn_path, STRERROR(errno));
	} else
		fprintf(stderr, "Unable to resolve %s : return value = %ld\n", GTCM_GNP_CMERR_FN, (long)rval);
	gtcm_firsterr = FALSE;
}
