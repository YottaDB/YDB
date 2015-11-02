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

#include "gtm_string.h"

#include "view.h"
#include "gtm_caseconv.h"

#define TOP_VIEWTAB_ENTRY (&viewtab[sizeof(viewtab) / sizeof(viewtab[0])])
#define VT_KWSIZE (sizeof(viewtab[0].keyword))

#define VIEWTAB(A,B,C,D) {A, B, C, D}
const static readonly viewtab_entry viewtab[] =
{
#include "viewtab.h"
};
#undef VIEWTAB

viewtab_entry *viewkeys(mstr *v)
{
/* given view keyword, return pointer to viewtab_entry for that keyword
   or: return 0 means not found, return -1 means keyword is ambiguous */

	unsigned char		cmpbuf[VT_KWSIZE];
	const viewtab_entry	*vt_ptr, *vt_top;
	short 			len;
	int 			n;

	error_def(ERR_VIEWNOTFOUND);
	error_def(ERR_VIEWAMBIG);

	if (v->len == 0)
		vt_ptr = (viewtab_entry *)NULL;
	else
	{
		len = (v->len < sizeof(cmpbuf) ? v->len : sizeof(cmpbuf));
		lower_to_upper(cmpbuf, (uchar_ptr_t)v->addr, len);
		vt_top = TOP_VIEWTAB_ENTRY;
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
					vt_ptr = (viewtab_entry *)-1;
				break;
			}
		}
	}
	if (vt_ptr == (viewtab_entry *)-1)
		rts_error(VARLSTCNT(4) ERR_VIEWAMBIG, 2, v->len, v->addr);
	else if (!vt_ptr || vt_ptr >= vt_top)
		rts_error(VARLSTCNT(4) ERR_VIEWNOTFOUND, 2, v->len, v->addr);

	return (viewtab_entry *)vt_ptr;
}
