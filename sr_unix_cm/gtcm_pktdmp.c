/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
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

#include "gtm_time.h"

#include "gtcm_sysenv.h"
#include "gtcm.h"
#include "omi.h"

GBLREF omi_conn		*curr_conn;
GBLREF char		*omi_service;

/* return in a static buffer the ASCII representation of a network address.
   Returned as:
   "hostid (nn.nn.nn.nn)" or "nn.nn.nn.nn" depending on whether or not
   the host is listed in /etc/hosts.
 */
char *gtcm_hname(struct sockaddr_in *sin)
{
    struct hostent	*he;
    static char name[256];

#ifndef SUNOS
    if ((he = gethostbyaddr((void *)&sin->sin_addr.s_addr,
			    sizeof(struct in_addr), AF_INET)))
	sprintf(name,"%s (%d.%d.%d.%d)",he->h_name,
		   sin->sin_addr.s_addr >> 24,
		   sin->sin_addr.s_addr >> 16 & 0xFF,
		   sin->sin_addr.s_addr >> 8 & 0xFF,
		   sin->sin_addr.s_addr & 0xFF);
    else
	sprintf(name,"%d.%d.%d.%d",
		   sin->sin_addr.s_addr >> 24,
		   sin->sin_addr.s_addr >> 16 & 0xFF,
		   sin->sin_addr.s_addr >> 8 & 0xFF,
		   sin->sin_addr.s_addr & 0xFF);
#else
    sprintf(name,"%d.%d.%d.%d",
	    sin->sin_addr.s_addr >> 24,
	    sin->sin_addr.s_addr >> 16 & 0xFF,
	    sin->sin_addr.s_addr >> 8 & 0xFF,
	    sin->sin_addr.s_addr & 0xFF);
#endif
    return name;
}




/* dump packet with connection information...*/
void gtcm_cpktdmp(char *ptr, int length, char *msg)
{
    char newmsg[512];
    struct hostent *peer;

    if (curr_conn && (512-strlen(msg)) > 25)
    {
	sprintf(newmsg,"Conn: %s - %s",
		gtcm_hname(&curr_conn->stats.sin), msg);

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
	char fileName[256];
	time_t ctim;
	struct tm *ltime;
	FILE *fp;
	char *gtm_dist;

	ctim = time(0);
	ltime = localtime(&ctim);
	sprintf(tbuf, "%02d%02d%02d%02d",ltime->tm_mon + 1,ltime->tm_mday,
		ltime->tm_hour,ltime->tm_min);

	if (gtm_dist=getenv("gtm_dist"))
	{
	    	char subdir[256];
		struct stat buf;

		/* check for the subdirectory $gtm_dist/log/<omi_service>
		 * If the subdirectory exists, place the log file there.
		 * Otherwise...place the file in $gtm_dist/log.
		 */
		sprintf(subdir,"%s/log/%s", gtm_dist, omi_service);
		if (stat(subdir,&buf) == 0
		    && S_ISDIR(buf.st_mode))
		{
		    sprintf(fileName,"%s/%s_%s.%d", subdir, omi_service,
			tbuf, fileID++);
		}
		else
		{
		    sprintf(fileName,"%s/log/%s_%s.%d", gtm_dist, omi_service,
			    tbuf, fileID++);
		}
	}
	else
		sprintf(fileName,"/usr/tmp/%s_%s.%d", omi_service,
			tbuf, fileID++);


	fp = fopen(fileName, "w");
	if (fp == NULL)
	{
		fprintf(stderr,"Could not open packet dump file (%s).\n",fileName);
		perror(fileName);
		return;
	}

	OMI_DBG((omi_debug, "%s\n", msg));
	OMI_DBG((omi_debug, "Log dumped to %s.\n", fileName));

	fprintf(fp,"%s\n", msg);

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
		    fprintf(fp,"%08x ", buf[j]);
		for (j = 0; j < 16; j++)
		    if (j >= len)
			chr[j] = ' ';
		    else if (chr[j] < 32 || chr[j] > 126)
			chr[j] = '.';
		fprintf(fp,"%16s %x\n", chr, offset);
	    }
	fflush(fp);
	fclose(fp);
}
