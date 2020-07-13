/****************************************************************
 *								*
 * Copyright (c) 2001-2020 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "compiler.h"
#include "opcode.h"
#include "toktyp.h"
#include "nametabtyp.h"
#include "gtm_caseconv.h"
#include "namelook.h"

/* given offest table, name table, string and its length, returns index associated with matching entry or -1 for not found */

int namelook(const unsigned char offset_tab[], const nametabent *name_tab, char *str, int strlength)
{
	const nametabent	*top, *i;
	uint4			find;
	unsigned char		temp[NAME_ENTRY_SZ], x;

	if (strlength > NAME_ENTRY_SZ)
		return -1;
	lower_to_upper(&temp[0], (uchar_ptr_t)str, strlength);
	if ('%' == (x = temp[0]))								/* WARNING assignment */
		return -1;
	i = name_tab + offset_tab[x -= 'A'];
	top = name_tab + offset_tab[++x];
	assert((i == top) || (i->name[0] >= temp[0]));
	if ('Z' == temp[0])
	{	/* relaxed conventions for Z* keyword abbreviations accept leading subsets */
		for (; i < top; i++)
		{
			if (!(find = memcmp(&temp[0], i->name, strlength)))			/* WARNING assignment */
				return (int)(i - name_tab);
			if (0 > find)
				return -1;							/* assumes alpha ordering */
		}
	} else
	{
		for (; i < top; i++)
		{
			if ((strlength == i->len) || ((strlength > i->len) && ('*' == i->name[i->len])))
			{
				if (!(find = memcmp(&temp[0], i->name, (int4)(i->len))))	/* WARNING assignment */
					return (int)(i - name_tab);
				if (0 > find)
					return -1;						/* assumes alpha ordering */
			}
		}
	}
	return -1;
}
