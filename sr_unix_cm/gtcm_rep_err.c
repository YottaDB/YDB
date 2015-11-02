/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc *
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

#include "gtm_string.h"
#include "gtm_stdio.h"
#include "gtm_stdlib.h"
#include "gtm_syslog.h"
#include "gtm_time.h"

#include "gtcm.h"
#include "fao_parm.h"
#include "eintr_wrappers.h"
#include "sgtm_putmsg.h"
#ifdef __MVS__
#include "gtm_stat.h"
#include "gtm_zos_io.h"
#endif

GBLREF char		*omi_service;
static boolean_t 	first_syslog = TRUE;

void gtcm_rep_err(char *msg, int errcode)
{
    FILE	*fp;
    char	outbuf[2048];
    time_t	now;
    int		status;
    char *gtm_dist, fileName[256];

    error_def(ERR_TEXT);
    ZOS_ONLY(error_def(ERR_BADTAG);)
    /*error_def(ERR_OMISERVHANG);*/

    if ('\0' == msg[0])
   	sgtm_putmsg(outbuf, VARLSTCNT(2) errcode, 0);
    else
   	sgtm_putmsg(outbuf, VARLSTCNT(6) errcode, 0, ERR_TEXT, 2, LEN_AND_STR(msg));


    if (gtm_dist = GETENV("gtm_dist"))
	    SPRINTF(fileName,"%s/log/gtcm_server.erlg", gtm_dist);
    else
	    SPRINTF(fileName, "%s/log/gtcm_server.erlg", P_tmpdir);

#ifdef __MVS__
    if (-1 != gtm_zos_create_tagged_file(fileName, TAG_EBCDIC))
    {
	char *tag_emsg = STRERROR(errno);
	sgtm_putmsg(outbuf, VARLSTCNT(10) ERR_BADTAG, 4, LEN_AND_STR(fileName),
		-1, TAG_EBCDIC, ERR_TEXT, 2, RTS_ERROR_STRING(tag_emsg));
    }
#endif
    if ((fp = Fopen(fileName, "a")))
    {
		now=time(0);
		FPRINTF(fp, "%s", GTM_CTIME(&now));
		FPRINTF(fp,"server(%s)  %s", omi_service, outbuf);
		FCLOSE(fp, status);
    }

#ifdef BSD_LOG
    if (first_syslog)
    {
		first_syslog = FALSE;
		(void)OPENLOG("GTCM", LOG_PID | LOG_CONS | LOG_NOWAIT, LOG_USER);
    }
    SYSLOG(LOG_ERR, "%s", outbuf);
#endif /* defined(BSD_LOG) */

    return;
}
