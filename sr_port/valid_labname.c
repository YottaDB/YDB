/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "toktyp.h"
#include "valid_mname.h"

boolean_t valid_labname(mstr *targ)
{
	char	*src, *src_top;

	src = targ->addr;
	src_top = targ->addr + targ->len;
	if (0 < targ->len && targ->len <= MAX_MIDENT_LEN)
	{
		if (VALID_MNAME_FCHAR(*src))
		{
			for ( ; ++src < src_top; )
			{
				if (!VALID_MNAME_NFCHAR(*src))
					break;
			}
			return (src == src_top ? TRUE: FALSE); /* Was it a valid M identifier? */
		} else if ((unsigned char)(*src) < NUM_ASCII_CHARS && ctypetab[*src] == TK_DIGIT)
		{
			for ( ; ++src < src_top; )
			{
				if ((unsigned char)(*src) >= NUM_ASCII_CHARS || ctypetab[*src] != TK_DIGIT)
					break;
			}
			return (src == src_top ? TRUE: FALSE); /* Was it a valid M identifier? */
		}
	}
	return FALSE;
}
