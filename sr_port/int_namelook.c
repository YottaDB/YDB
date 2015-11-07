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

#include "gtm_string.h"

#include "compiler.h"
#include "opcode.h"
#include "toktyp.h"
#include "nametabtyp.h"
#include "gtm_caseconv.h"
#include "int_namelook.h"
/* This routine performs the same function as namelook() only the first parameter for this version
   is an array of type uint4 vs unsigned char in namelook().  This permits a much larger number of
   deviceparameter names.  The purpose of this function is to search for the character string in
   in the 3rd input parameter called str.  It can find either an exact match or a match which has an *
   as its final character.  The table containing names to compare is the 2nd parameter called nametabent.
   If the name is found then the offset into the nametabent is returned.  If not found, a -1 is returned.
 */

int int_namelook(const uint4 offset_tab[], const nametabent *name_tab, char *str, int strlength)
{
	unsigned char		temp[NAME_ENTRY_SZ], x;
	const nametabent	*top, *i;

	if (strlength > NAME_ENTRY_SZ)
		return -1;
	lower_to_upper(&temp[0], (uchar_ptr_t)str, strlength);
	if ('%' == (x = temp[0]))
		return -1;
	i = name_tab + offset_tab[x -= 'A'];
	top = name_tab + offset_tab[++x];
	assert((i == top) || (i->name[0] >= temp[0]));
	for (; i < top; i++)
	{
		if ((strlength == i->len) || ((strlength > i->len) && ('*' == i->name[i->len])))
		{
			if (!memcmp(&temp[0], i->name, (int4)(i->len)))
				return (int)(i - name_tab);
		}
	}
	return -1;
}
