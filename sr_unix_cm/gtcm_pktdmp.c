/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
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

#include "gtm_string.h"
#include "gtm_stdio.h"
#include "gtm_stat.h"
#include "gtm_stdlib.h"
#include "gtm_limits.h"

#include "gtm_time.h"
#include "gtmio.h"
#include "have_crit.h"

#include "gtcm_sysenv.h"
#include "gtcm.h"
#include "omi.h"
#ifdef __MVS__
#include "eintr_wrappers.h"
#include "gtm_zos_io.h"
#endif
#include "ydb_getenv.h"

GBLREF omi_conn		*curr_conn;
GBLREF char		*omi_service;

error_def(ERR_GETADDRINFO);
error_def(ERR_GETNAMEINFO);
error_def(ERR_TEXT);

/* return in a static buffer the ASCII representation of a network address.
   Returned as:
   "hostid (nn.nn.nn.nn)" or "nn.nn.nn.nn" depending on whether or not
   the host is listed in /etc/hosts.
 */
char *gtcm_hname(struct addrinfo *ai_ptr)
{
	static char name[NI_MAXHOST + NI_MAXSERV + 4];
	char ipname[NI_MAXSERV];
	char host[NI_MAXHOST];
	int	errcode;

	ASSERT_IS_LIBGTCM;
	if(0 != (errcode = getnameinfo(ai_ptr->ai_addr, ai_ptr->ai_addrlen, ipname,
					 SIZEOF(ipname), NULL, 0, NI_NUMERICHOST)))
	{
		RTS_ERROR_ADDRINFO(NULL, ERR_GETNAMEINFO, errcode);
		return NULL;
	}
	if(0 == (errcode = getnameinfo(ai_ptr->ai_addr, ai_ptr->ai_addrlen, host, SIZEOF(host),
					NULL, 0, NI_NAMEREQD)))
		SNPRINTF(name, NI_MAXHOST + NI_MAXSERV + 4, "%s (%s)", host, ipname);
	else
		SNPRINTF(name, NI_MAXHOST + NI_MAXSERV + 4, "%s", ipname);
	return name;
}




/* dump packet with connection information...*/
void gtcm_cpktdmp(char *ptr, int length, char *msg)
{
    char newmsg[512];
    struct hostent *peer;

	ASSERT_IS_LIBGTCM;
    if (curr_conn && (512-strlen(msg)) > 25)
    {
	SNPRINTF(newmsg, 512,"Conn: %s - %s",
		gtcm_hname(&curr_conn->stats.ai), msg);

	gtcm_pktdmp(ptr, length, newmsg);
    }
    else
	gtcm_pktdmp(ptr, length, msg);
}

void gtcm_pktdmp(char *ptr, int length, char *msg)
{

	char *end;
	char *chr;
	int buf[5];
	int len;
	int j;
	int offset = 0;
	static int fileID = 0;
	char tbuf[16];
	char fileName[YDB_PATH_MAX];
	time_t ctim;
	struct tm *ltime;
	FILE *fp;
	char *ydb_dist;
	int status;

	ASSERT_IS_LIBGTCM;
	ctim = time(0);
	GTM_LOCALTIME(ltime, &ctim);
	SNPRINTF(tbuf, 16, "%02d%02d%02d%02d",ltime->tm_mon + 1,ltime->tm_mday,
		ltime->tm_hour,ltime->tm_min);

	if (ydb_dist = ydb_getenv(YDBENVINDX_DIST_ONLY, NULL_SUFFIX, NULL_IS_YDB_ENV_MATCH))
	{
	    	char subdir[YDB_PATH_MAX];
		struct stat buf;

		/* check for the subdirectory $ydb_dist/log/<omi_service>
		 * If the subdirectory exists, place the log file there.
		 * Otherwise...place the file in $ydb_dist/log.
		 */
		SNPRINTF(subdir, YDB_PATH_MAX, "%s/log/%s", ydb_dist, omi_service);
		if (stat(subdir,&buf) == 0 && S_ISDIR(buf.st_mode))
		{
		    SNPRINTF(fileName, YDB_PATH_MAX,"%s/%s_%s.%d", subdir, omi_service,
			tbuf, fileID++);
		} else
		{
		    SNPRINTF(fileName, YDB_PATH_MAX,"%s/log/%s_%s.%d", ydb_dist, omi_service,
			    tbuf, fileID++);
		}
	} else
		SNPRINTF(fileName, YDB_PATH_MAX,"/usr/tmp/%s_%s.%d", omi_service,
			tbuf, fileID++);

#ifdef __MVS__
	if (-1 == gtm_zos_create_tagged_file(fileName, TAG_EBCDIC))
	{
		FPRINTF(stderr,"Could not create and tag new packet dump file (%s).\n", fileName);
		perror(fileName);
	}
#endif
	Fopen(fp, fileName, "w");
	if (fp == NULL)
	{
		FPRINTF(stderr,"Could not open packet dump file (%s).\n", fileName);
		perror(fileName);
		return;
	}

	OMI_DBG((omi_debug, "%s\n", msg));
	OMI_DBG((omi_debug, "Log dumped to %s.\n", fileName));

	FPRINTF(fp,"%s\n", msg);

	buf[4] = '\0';

	end = ptr + length;
	chr = (char *)buf;
	    while (ptr < end) {
		fputc('\t', fp);
		if ((len = (int)(end - ptr)) > 16)
		    len = 16;
		memcpy(chr, ptr, len);
		ptr += len;
		offset += len;
		for (j = len; j < 16; j++)
		    chr[j] = '\0';
		for (j = 0; j < 4; j++)
		    FPRINTF(fp,"%08x ", buf[j]);
		for (j = 0; j < 16; j++)
		    if (j >= len)
			chr[j] = ' ';
		    else if (chr[j] < 32 || chr[j] > 126)
			chr[j] = '.';
		FPRINTF(fp,"%16s %x\n", chr, offset);
	    }
	FFLUSH(fp);
	FCLOSE(fp, status);
}
