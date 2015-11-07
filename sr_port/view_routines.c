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

#include <rtnhdr.h>
#include "stack_frame.h"
#include "gtm_caseconv.h"
#include "stringpool.h"
#include "view.h"
#include "rtn_src_chksum.h"

GBLREF rtn_tabent	*rtn_names, *rtn_names_end;

/*
 * Returns name of next (alphabetical) routine currently ZLINK'd.
 * If no such routine, returns null string.
 */

void view_routines(mval *dst, mident_fixed *name)
{
	mident		rname;
	rtn_tabent	*mid;
	boolean_t	found;

	rname.len = INTCAST(mid_len(name));	/* convert from mident_fixed to mident */
	rname.addr = &name->c[0];
	found = find_rtn_tabent(&mid, &rname);
	if (found)
		mid++;	/* want the *next* routine */
	/* Skip over all routines that are not created by the user. These routines include
	 * the dummy null routine, $FGNXEC (created for DALs) and $FGNFNC (created for Call-ins)
	 * which are guaranteed to sort before any valid M routine */
	while ((mid <= rtn_names_end) && ((0 == mid->rt_name.len) || ('%' > mid->rt_name.addr[0])))
		mid++;
	if (mid > rtn_names_end)
		dst->str.len = 0;
	else
	{
		dst->str.addr = mid->rt_name.addr;
		dst->str.len = mid->rt_name.len;
		s2pool(&dst->str);
	}
}

/*
 * Returns checksum of routine <name>.
 * If name is not ZLINK'd, returns null string.
 */

void view_routines_checksum(mval *dst, mident_fixed *name)
{
	mident		rname;
	rtn_tabent	*mid;
	boolean_t	found;
	char		buf[MAX_ROUTINE_CHECKSUM_DIGITS];

	rname.len = INTCAST(mid_len(name));	/* convert from mident_fixed to mident */
	rname.addr = &name->c[0];
	found = find_rtn_tabent(&mid, &rname);
	/* Ignore the dummy null routine, $FGNXEC (created for DALs) and $FGNFNC (created for Call-ins) */
	if (!found || ((0 == mid->rt_name.len) || ('%' > mid->rt_name.addr[0])))
		dst->str.len = 0;
	else
	{
		dst->str.addr = &buf[0];
		dst->str.len = append_checksum((unsigned char *)&buf[0], mid->rt_adr);
		s2pool(&dst->str);
	}
}
