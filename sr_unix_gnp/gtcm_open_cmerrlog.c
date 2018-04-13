/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2018 YottaDB LLC. and/or its subsidiaries.*
 * All rights reserved.						*
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
#include "ydb_trans_log_name.h"
#include "gtm_string.h"
#include "gtcm_open_cmerrlog.h"
#include "iosp.h"
#include "gtm_rename.h"
#ifdef __MVS__
#include "gtm_zos_io.h"
#endif
#include "eintr_wrappers.h"
#include "have_crit.h"

#define GTCM_GNP_CMERR_FN		"/gtcm_gnp_server.log"

GBLREF bool	gtcm_errfile;
GBLREF bool	gtcm_firsterr;
GBLREF FILE	*gtcm_errfs;
GBLREF char	gtcm_gnp_server_log[];
GBLREF int	gtcm_gnp_log_path_len;

error_def(ERR_TEXT);

void gtcm_open_cmerrlog(void)
{
	int		len;
	mstr		lfn1, lfn2;
	char		lfn_path[MAX_TRANS_NAME_LEN + 1];
	char		new_lfn_path[MAX_TRANS_NAME_LEN + 1];
	int		new_len;
	uint4 		ustatus;
	int4 		rval, status;
	FILE 		*new_file;

	ZOS_ONLY(error_def(ERR_BADTAG);)

	ASSERT_IS_LIBGNPSERVER;
	if (0 != (len = STRLEN(gtcm_gnp_server_log)))
	{
		lfn1.addr = gtcm_gnp_server_log;
		lfn1.len = len;
		rval = trans_log_name(&lfn1, &lfn2, lfn_path, SIZEOF(lfn_path), do_sendmsg_on_log2long);
	} else
	{	/* Check if $ydb_log/$gtm_log is defined. If so first find that expanded value. */
		rval = ydb_trans_log_name(YDBENVINDX_LOG, &lfn2, lfn_path, SIZEOF(lfn_path), IGNORE_ERRORS_TRUE, NULL);
		if ((SS_NORMAL == rval) || (SS_NOLOGNAM == rval))
		{	/* Add a "/gtcm_gnp_server.log" suffix to the buffer that already has the evaluated env var */
			len = SNPRINTF(lfn2.addr + lfn2.len, SIZEOF(lfn_path) - lfn2.len, "%s", GTCM_GNP_CMERR_FN);
			if (0 > len)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
					RTS_ERROR_LITERAL("SNPRINTF)"), CALLFROM, errno);
			len += lfn2.len;
			if (len >= SIZEOF(lfn_path))	/* Output from SNPRINTF was truncated. */
				len = SIZEOF(lfn_path);
			lfn2.len = len;
		}
	}
	if ((SS_NORMAL == rval) || (SS_NOLOGNAM == rval))
	{
		lfn_path[lfn2.len] = 0;
		new_len = ARRAYSIZE(new_lfn_path);
		rename_file_if_exists(lfn_path, lfn2.len, new_lfn_path, &new_len, &ustatus);
#		ifdef __MVS__
		if (-1 == gtm_zos_create_tagged_file(lfn_path, TAG_EBCDIC))
			TAG_POLICY_GTM_PUTMSG(lfn_path, errno, -1, TAG_EBCDIC);
#		endif
		Fopen(new_file, lfn_path, "a");
		if (NULL != new_file)
		{
			gtcm_errfile = TRUE;
			if (gtcm_errfs)
				FCLOSE(gtcm_errfs, status);
			gtcm_errfs = new_file;
			DUP2(fileno(gtcm_errfs), 1, status);
			if (status < 0)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_TEXT, 2,
						LEN_AND_LIT("Error on dup2 of stdout"), errno);
			DUP2(fileno(gtcm_errfs), 2, status);
			if (status < 0)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_TEXT, 2,
						LEN_AND_LIT("Error on dup2 of stderr"), errno);
		} else
			fprintf(stderr, "Unable to open %s : %s\n", lfn_path, STRERROR(errno));
	} else
		fprintf(stderr, "Unable to resolve %s : return value = %ld\n", GTCM_GNP_CMERR_FN, (long)rval);
	gtcm_firsterr = FALSE;
}
