/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "cli.h"
#include "dse.h"

int dse_data(char *dst, int *len)
{

	unsigned short cli_len;
	char buf[MAX_LINE],*src,*bot,*top;

	cli_len = SIZEOF(buf);
	if (!cli_get_str("DATA",buf,&cli_len))
		return FALSE;
	bot = dst;
	top = &buf[cli_len - 1];
	src = &buf[0];

#ifdef VMS
	if (buf[0] == '"')
		src = &buf[1];
#endif

	for (; src <= top ;src++)
	{
#ifdef VMS
		if (src == top && *src == '"')
			break;
#endif
		if (*src == '\\')
		{
			src++;
			if (*src == '\\')
			{
				*dst++ = '\\';
				continue;
			}
			if (*src >= '0' && *src <= '9')
				*dst = *src - '0';
			else if (*src >= 'a' && *src <= 'f')
				*dst = *src - 'a' + 10;
			else if (*src >= 'A' && *src <= 'F')
				*dst = *src - 'A' +10;
			else
				continue;
			src++;
			if (*src >= '0' && *src <= '9')
				*dst = (*dst << 4) + *src - '0';
			else if (*src >= 'a' && *src <= 'f')
				*dst = (*dst << 4) + *src - 'a' + 10;
			else if (*src >= 'A' && *src <= 'F')
				*dst = (*dst << 4) + *src - 'A' +10;
			dst++;
		}
		else
			*dst++ = *src;
	}
	*len = (int)(dst - bot);
	return TRUE;

}
