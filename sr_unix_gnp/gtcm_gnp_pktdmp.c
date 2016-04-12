/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "cmidef.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtm_stat.h"
#include "gtm_stdlib.h"
#include "gtm_time.h"
#include "gtmio.h"
#include "have_crit.h"
#include "gtcm_gnp_pktdmp.h"

/* dump packet with connection information...*/
void gtcm_gnp_cpktdmp(FILE *fp, struct CLB *lnk, int sta, unsigned char *buf, size_t len, char *msg)
{
    char newmsg[512];

    if ((SIZEOF(newmsg)-strlen(msg)) > 25)
    {
    	newmsg[SIZEOF(newmsg)-1] = '\0';
    	strcpy(newmsg, "Client: ");
	cmi_peer_info(lnk, &newmsg[strlen(newmsg)], SIZEOF(newmsg)-strlen(newmsg)-1);
	if (strlen(newmsg) && strlen(newmsg) < (strlen(msg) + 3))
	{
		strcat(newmsg, " - ");
		strcat(newmsg, msg);
	}
	gtcm_gnp_pktdmp(fp, lnk, sta, buf, len, newmsg);
    }
    else
	gtcm_gnp_pktdmp(fp, lnk, sta, buf, len, msg);
}

void gtcm_gnp_pktdmp(FILE *fp, struct CLB *lnk, int sta, unsigned char *buf, size_t len, char *msg)
{

	char *op;
	unsigned char *chr;
	int count = (int)(len);
	int j;
	int offset = 0;
	time_t ctim;
	struct tm *ltime;
	static char *digs = "0123456789ABCDEF";
	unsigned nibble;
	char linebuf[32 + 1 + 16 + 1];

	switch (sta)
	{
	case CM_CLB_WRITE:
		op = "write";
		break;
	case CM_CLB_WRITE_URG:
		op = "writeurg";
		break;
	case CM_CLB_READ_URG:
		op = "readurg";
		break;
	case CM_CLB_READ:
		op = "read";
		break;
	case CM_CLB_DISCONNECT:
		op = "disconnect";
		break;
	default:
		op = "*unknown*";
		break;
	}
	chr = buf;
	ctim = time(NULL);
	GTM_LOCALTIME(ltime, &ctim);
	FPRINTF(fp, "%04d%02d%02d%02d%02d%02d (%s) %s length %d\n",
		ltime->tm_year+1900, ltime->tm_mon + 1, ltime->tm_mday,
		ltime->tm_hour,ltime->tm_min, ltime->tm_sec, op, msg, count);

	while (count >= 16) {
		linebuf[32] = ' ';
		for (j = 0; j < 16;j++)
		{
			nibble = (chr[j] >> 4) & 0x0f;
			linebuf[j + j] = digs[nibble];
			nibble = chr[j] & 0x0f;
			linebuf[j + j + 1] = digs[nibble];
			linebuf[j + 32 + 1] = ISPRINT_ASCII(chr[j])? chr[j]: '.';
		}
		linebuf[32 + 1 + 16] = '\0';
		FPRINTF(fp, "%.8X %s\n", offset, linebuf);
		count -= 16;
		offset += 16;
		chr += 16;
	}
	if (count)
	{
		/* remainder on last line */
		linebuf[32] = ' ';
		for (j = 0; j < count;j++)
		{
			nibble = (chr[j] >> 4) & 0x0f;
			linebuf[j + j] = digs[nibble];
			nibble = chr[j] & 0x0f;
			linebuf[j +j +1] = digs[nibble];
			linebuf[j + 32 + 1] = ISPRINT_ASCII(chr[j])? chr[j]: '.';
		}
		memset(&linebuf[count + count], ' ' , 2 * (16 - count));
		memset(&linebuf[32 + 1 + count], ' ', 16 - count);
		linebuf[32 + 1 + 16] = '\0';
		FPRINTF(fp, "%.8X %s\n", offset, linebuf);
	}
	FFLUSH(fp);
}
