/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "view.h"
#include "gtm_caseconv.h"
#include "gdsroot.h"		/* Added to support alias.h */
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "alias.h"		/* Needed for DEBUG_ALIAS flag used in viewtab.h */

#define VT_KWSIZE (SIZEOF(viewtab[0].keyword))

#define VIEWTAB(A,B,C,D) {A, B, C, D}
const static readonly viewtab_entry viewtab[] =
{
#include "viewtab.h"
};
#undef VIEWTAB

error_def(ERR_VIEWNOTFOUND);
error_def(ERR_VIEWAMBIG);

viewtab_entry *viewkeys(mstr *v)
{	/* given view keyword, return pointer to viewtab_entry for that keyword
	 * or: return 0 means not found, return -1 means keyword is ambiguous.
	 */

	unsigned char		cmpbuf[VT_KWSIZE];
	const viewtab_entry	*vt_ptr, *vt_top;
	short 			len;
	int 			n;

	if (v->len == 0)
		vt_ptr = (viewtab_entry *)NULL;
	else
	{
		len = (v->len < SIZEOF(cmpbuf) ? v->len : SIZEOF(cmpbuf));
		lower_to_upper(cmpbuf, (uchar_ptr_t)v->addr, len);
		vt_top = ARRAYTOP(viewtab);
		for (vt_ptr = viewtab ; vt_ptr < vt_top ; vt_ptr++)
		{
			n = memcmp(vt_ptr->keyword, cmpbuf, len);
			if (n > 0)
			{	vt_ptr = 0;
				break;
			}
			else if (n == 0)
			{
				if (vt_ptr < vt_top - 1 && memcmp(cmpbuf, (vt_ptr + 1)->keyword, len) == 0)
					vt_ptr = (viewtab_entry *)-1L;
				break;
			}
		}
	}
	if (vt_ptr == (viewtab_entry *)-1L)
		rts_error(VARLSTCNT(4) ERR_VIEWAMBIG, 2, v->len, v->addr);
	else if (!vt_ptr || vt_ptr >= vt_top)
		rts_error(VARLSTCNT(4) ERR_VIEWNOTFOUND, 2, v->len, v->addr);
	return (viewtab_entry *)vt_ptr;
}
