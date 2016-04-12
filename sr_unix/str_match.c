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

/* str_match.c */

#include "mdef.h"

#include "str_match.h"

char *mystrstr(char *o, unsigned short olen, char *t, unsigned short tlen);

char *mystrstr(char *o, unsigned short olen, char *t, unsigned short tlen)
{
        char            *ptr;
        unsigned short  length;
        bool            matched = FALSE;

        if (olen < tlen)
                return NULL;

        for (ptr = o; ptr <= o + olen - tlen; ptr++)
        {
                length = 0;
                matched = TRUE;
                for (length = 0; length < tlen; length++)
                {
                        if (('?' != *(t + length)) && (*(ptr + length) != *(t + length)))
                        {
                                matched = FALSE;
                                break;
                        }
                }
                if (TRUE == matched)
                        return ptr;
        }

        return NULL;
}

bool str_match(char *ori, unsigned short orilen, char *template, unsigned short template_len)
{
	char 		*c, *c_top, *start, *c_prev;
	template_struct	temps;
	unsigned short	counter = 0, i, len, prev_counter;
	bool		wild = FALSE;

	/* ================== Analyze the template string ============================== */

	for (c = template, c_top = template + template_len; c < c_top;)
	{
		c_prev = c;
		while (c < c_top && '*' == *c)
			c++;
		if (c > c_prev)
		{
			temps.sub[counter].addr = NULL;
			temps.sub[counter].len = 0;
			counter++;
			c_prev = c;
		}
		while (c < c_top && '*' != *c)
			c++;
		if (c > c_prev)
		{
			temps.sub[counter].addr = c_prev;
			temps.sub[counter].len = INTCAST(c - c_prev);
			counter++;
		}
	}
	temps.n_subs = counter;

	/* ================== Match the original string ================================ */

	c_prev = ori;
	c_top = ori + orilen;

	for(i = 0; i < temps.n_subs; i++)
	{
		if (0 == temps.sub[i].len)
		{
			wild = TRUE;
			continue;
		}
		else
		{
			if ((NULL == (c = mystrstr(c_prev, c_top - c_prev, temps.sub[i].addr, temps.sub[i].len)))
				|| ((c_prev != c) && (TRUE != wild)))
				return FALSE;
			c_prev = c + temps.sub[i].len;
			wild = FALSE;
		}
	}

	if ((c_prev != c_top) && (TRUE != wild))
		return FALSE;

	return TRUE;
}
