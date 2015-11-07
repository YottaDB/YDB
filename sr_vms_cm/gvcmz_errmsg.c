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
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"

gvcmz_errmsg(c,close)
struct CLB *c;
bool close;
{
	char *buf1,*buf2,*buf3,*ptr;
	int  i,j,x,count1,count2,count3;
	int4 *msgbuf,*msgptr;
	uint4 status;
	cmi_descriptor *desc;

	buf1 = c->mbf;
	buf2 = buf3 = 0;
	count2 = count3 = 0;
	ptr = buf1 + 1;
	if (*ptr)
	{	buf2 = malloc(c->mbl);
		c->mbf = buf2;
		status = cmi_read(c);
		if ((status & 1) == 0)
		{	buf2 = 0;
			count2 = 1;
			goto signal;
		}
		if (*c->mbf != CMMS_E_ERROR)
		{	buf2 = 0;
			count2 = 1;
			goto signal;
		}
		ptr = buf2 + 1;
		if (*ptr)
		{	buf3 = malloc(c->mbl);
			c->mbf = buf3;
			status = cmi_read(c);
			if ((status & 1) == 0)
			{	buf3 = 0;
				count3 = 1;
				goto signal;
			}
			if (*c->mbf != CMMS_E_ERROR)
			{	buf3 = 0;
				count3 = 1;
				goto signal;
			}
		}
	}
	ptr = buf1 + 2;
	count1 = *ptr++;
	if (buf2)
	{	ptr = buf2 + 2;
		count2 = *ptr;
		if (buf3)
		{	ptr = buf3 + 2;
			count3 = *ptr;
		}
		ptr = buf1 + 3;
	}
signal:
	msgbuf = malloc((count1 + count2 + count3 + 1) * SIZEOF(int4));
	msgptr = msgbuf;
	*msgptr++ = count1 + count2 + count3;
	x = count1;
	for ( j = 0; ; j++)
	{	for ( i = 0; i < x ; i++)
		{	if (*ptr == 'L')
			{	ptr++;
				*msgptr++ = *(int4 *)ptr;
				ptr += SIZEOF(int4);
			}else if (*ptr == 'Q')
			{	ptr++;
				*msgptr++ = ptr;
				ptr += 4 * SIZEOF(int4);
			}else if (*ptr == 'C')
			{	ptr++;
				*msgptr++ = ptr + 1;
				ptr += 2 + *(short*)ptr;
			}else if (*ptr == 'A')
			{	ptr++;
				*msgptr++ = ptr + 2;
				ptr += 2 + *(short*)ptr;
			}else if (*ptr == 'D')
			{	desc = ptr + 1;
				desc->dsc$a_pointer = ptr + SIZEOF(cmi_descriptor) + 1;
				*msgptr++ = desc;
				ptr += 1 + SIZEOF(cmi_descriptor) + desc->dsc$w_length;
			}else
			{	if (j == 0)
					*msgbuf = i;
				else if (j == 1)
					*msgbuf = i + count1;
				else
					*msgbuf = i + count1 + count2;
				j = 2;
				break;
			}
		}
		if (j == 0 && buf2)
		{	x = count2;
			ptr = buf2 + 3;
		}else if (j == 1 && buf3)
		{	x = count3;
			ptr = buf3 + 3;
		}else
		{	break;
		}
	}
	if (buf2)
		free(buf2);
	if (buf3)
		free(buf3);
	c->mbf = buf1;
	if (close)
	{	gvcmy_close(c);
	}
	callg_signal(msgbuf);
}
