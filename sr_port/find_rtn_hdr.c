/****************************************************************
 *								*
 *	Copyright 2001, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include <rtnhdr.h>
#include "ident.h"
#include "min_max.h"
#include "stack_frame.h"
#include "compiler.h"	/* for WANT_CURRENT_RTN_MSTR macro */

GBLREF rtn_tabent	*rtn_names, *rtn_names_end;
GBLREF stack_frame	*frame_pointer;

rhdtyp	*find_rtn_hdr(mstr *name)
{
	rtn_tabent	*rtabent;

	if (WANT_CURRENT_RTN_MSTR(name))	/* want the *current* version on the stack, not the *newest* ZLINK'd version */
		return CURRENT_RHEAD_ADR(frame_pointer->rvector);
	if (find_rtn_tabent(&rtabent, name))
		return rtabent->rt_adr;
	else
		return NULL;
}

/*
 * Returns TRUE if a rtn_tabent exists for routine <name>, i.e. if
 * 	<name> is currently ZLINK'd.
 * Returns FALSE otherwise.
 * In either case, also "returns" (via <res>) the rtn_tabent
 * 	corresponding to the first routine name greater than or equal to
 * 	<name>. This is useful for looking up trigger routines that
 * 	include runtime disambiguators in their names.
 */

boolean_t find_rtn_tabent(rtn_tabent **res, mstr *name)
{
	rtn_tabent	*bot, *top, *mid;
	int4		comp;
	mident		rtn_name;
	mident_fixed	rtn_name_buff;
	boolean_t	ret;
	int		len;

	len = name->len;
	rtn_name.len = MIN(MAX_MIDENT_LEN, len);
#ifdef VMS
	rtn_name.addr = &rtn_name_buff.c[0];
	CONVERT_IDENT(rtn_name.addr, name->addr, name->len);
#else
	rtn_name.addr = name->addr;
#endif
	bot = rtn_names + 1;	/* Exclude the first NULL entry */
	top = rtn_names_end + 1;/* Include the last entry */
	for ( ; ; )
	{
		if (bot == top)
			break;
		assert(bot < top);
		mid = bot + (top - bot) / 2;
		assert(mid >= bot);
		MIDENT_CMP(&mid->rt_name, &rtn_name, comp);
		if (0 == comp)
		{
			*res = mid;
			return TRUE;
		} else if (0 > comp)
		{
			bot = mid + 1;
			continue;
		} else
		{
			assert(mid < top);
			top = mid;
			continue;
		}
	}
	*res = bot;
	return FALSE;
}
