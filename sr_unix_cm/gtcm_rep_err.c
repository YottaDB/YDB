/****************************************************************
 *								*
 *	Copyright 2001, 2014 Fidelity Information Services, Inc *
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
 *  gtcm_rep_err.c ---
 *
 *	Error logging facility.
 *
 */

#ifndef lint
static char rcsid[] = "$Header:$";
#endif

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_stdlib.h"
#include "gtm_string.h"
#include "send_msg.h"
#include "iosp.h"
#include "util.h"
#include "trans_log_name.h"
#include "gtm_syslog.h"
#include "gtm_time.h"

#include "gtcm.h"
#include "fao_parm.h"
#include "eintr_wrappers.h"
#include "sgtm_putmsg.h"
#include "gtm_limits.h"
#ifdef __MVS__
#include "gtm_stat.h"
#include "gtm_zos_io.h"
#endif

#define GTCM_SERV_LOG "/log/gtcm_server.erlg"

GBLREF char		*omi_service;
GBLREF char		gtm_dist[GTM_PATH_MAX];
GBLREF boolean_t	gtm_dist_ok_to_use;
STATICDEF boolean_t 	first_error = TRUE;
STATICDEF char 		fileName[GTM_PATH_MAX];

error_def(ERR_TEXT);
error_def(ERR_DISTPATHMAX);
error_def(ERR_GTMDISTUNDEF);
error_def(ERR_GTMDISTUNVERIF);
ZOS_ONLY(error_def(ERR_BADTAG);)

void gtcm_rep_err(char *msg, int errcode)
{
	FILE	*fp;
	char	outbuf[OUT_BUFF_SIZE];
	time_t	now;
	int	status, retval, gtm_dist_len;
	char 	*filebuf, *tag_emsg, *tmp_time;
	mstr	tn;

	if ('\0' == msg[0])
		sgtm_putmsg(outbuf, VARLSTCNT(2) errcode, 0);
	else
		sgtm_putmsg(outbuf, VARLSTCNT(6) errcode, 0, ERR_TEXT, 2, LEN_AND_STR(msg));
	if (first_error)
	{
		first_error = FALSE;
		if (gtm_dist_ok_to_use)
			SNPRINTF(fileName, GTM_PATH_MAX, "%s%s", gtm_dist, GTCM_SERV_LOG);
		else
		{
			STRNLEN(gtm_dist, GTM_PATH_MAX, gtm_dist_len);
			if (gtm_dist_len)
			{
				if (GTM_DIST_PATH_MAX <= gtm_dist_len)
						send_msg_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_DISTPATHMAX, 1, GTM_DIST_PATH_MAX);
				else
					send_msg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_GTMDISTUNVERIF, 4,
							LEN_AND_STR(gtm_dist), LEN_AND_LIT("gtcm"));
			} else
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_GTMDISTUNDEF);
			SNPRINTF(fileName, GTM_PATH_MAX, "%s%s", P_tmpdir, GTCM_SERV_LOG);
		}
	}
#	ifdef __MVS__
	if (-1 != gtm_zos_create_tagged_file(fileName, TAG_EBCDIC))
	{
		tag_emsg = STRERROR(errno);
		sgtm_putmsg(outbuf, VARLSTCNT(10) ERR_BADTAG, 4, LEN_AND_STR(fileName),
			    -1, TAG_EBCDIC, ERR_TEXT, 2, RTS_ERROR_STRING(tag_emsg));
	}
#	endif
	if ((fp = Fopen(fileName, "a")))
	{
		now = time(0);
		GTM_CTIME(tmp_time, &now);
		FPRINTF(fp, "%s", tmp_time);
		FPRINTF(fp, "server(%s)  %s", omi_service, outbuf);
		FCLOSE(fp, status);
	}
	util_out_print(outbuf, OPER);	/* Same message goes out to operator log */
	return;
}
