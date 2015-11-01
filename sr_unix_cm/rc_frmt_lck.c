/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "copy.h"
#include "rc.h"
#include "gvsub2str.h"

#ifdef __STDC__
int rc_frmt_lck(char *c, int4 max_size, unsigned char *key, short key_len, short *subcnt)
#else
int rc_frmt_lck(c, max_size, key, key_len, subcnt)
char *c;
int4 max_size;
unsigned char *key;
short key_len;
short *subcnt;
#endif
/* return a pointer that points after the last char added */
{
	short		t_len;
	unsigned char	*g, *g_top;
	char		*length, *start;
	char		buff[MAX_ZWR_KEY_SZ], *b_top, *b, *c_top, *sub_start;

	c_top = c + max_size;
	g = key;
	g_top = g + key_len;
	length = c;
	c += 1;
	*c++ = '^';
	for (b = c ; (*c = *g++); c++)
	{	;
	}
	*length = (c - b) + 1;
	t_len = *length;
	*subcnt = 1;
	if (t_len > 9) /* GT.M does not support global names > 8 chars
			  (one char for "^") */
		return(-RC_KEYTOOLONG);
	if (g >= g_top)		/* no subscipts */
	{	return(t_len);
	}
	length = c++;

	for(;;)
	{
		*subcnt = *subcnt + 1;

		/* sanity check subscript before passing to gvsub2str */
		if (*g == '\0')		/* not a valid number or a string */
		    return -RC_BADXBUF;

		b_top = (char *)gvsub2str(g,(uchar_ptr_t)buff,FALSE);

		sub_start = c;
		for (b = buff; b < b_top;)
		{	if (c >= c_top)
			{
				return(-RC_SUBTOOLONG);
			}
			*c++ = *b++;
		}
		if (c >= c_top)
			return (-RC_SUBTOOLONG);

		*length = c - sub_start;
		t_len += *length;
		for(; *g++ ; )
			;
		if (g >= g_top)
			break;
		length = c++;
	}
	return(t_len);
}

