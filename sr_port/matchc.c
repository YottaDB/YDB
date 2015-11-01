/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "matchc.h"


/*
 * -----------------------------------------------
 * Pseudo equivalent of VAX matchc instruction
 *
 * Arguments:
 *	del_len	- delimiter length
 *	del_str - pointer to delimiter string
 *	src_len	- length of source string
 *	src_str	- pointer to source string
 *	res	- pointer to the result flag
 *
 * Return:
 *	pointer to next character after match substring
 *	in the source string, if found.  Otherwise src_str + src_len.
 *
 * Side effects:
 *	set res arg to:
 *		0 - match found
 *		1 - match not found
 *
 * -----------------------------------------------
 */
unsigned char *matchc(int del_len, unsigned char *del_str,
		      int src_len, unsigned char *src_str,
		      int *res)
{
	unsigned char *psrc, *pdel;
	int src_cnt, del_cnt;
	int tmp_src_cnt;

	psrc = src_str;
	pdel = del_str;

	src_cnt = src_len;
	del_cnt = del_len;

	if (del_cnt == 0)
		goto found_match;

	if (src_cnt < del_cnt)
	{
		psrc += src_len;
		goto nomatch;
	}

	while(src_cnt > 0)
	{	/* Quick Find 1st delimiter char */
		while(*psrc != *pdel)
		{
			psrc = ++src_str;
			if (0 >= --src_cnt)
				goto nomatch;
		}

		tmp_src_cnt = src_cnt;

		/* Found delimiter */
		while(*psrc++ == *pdel++)
		{	if (0 >= --del_cnt)
				goto found_match;
			if (0 >= --tmp_src_cnt)
				goto nomatch;
		}

		/* Match lost, goto next source character */
		psrc = ++src_str;
		src_cnt--;
		pdel = del_str;
		del_cnt = del_len;
	}
nomatch:
	*res = 1;
	return (psrc);

found_match:
	*res = 0;
	return (psrc);
}
