/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <rtnhdr.h>
#include "urx.h"

GBLDEF urx_rtnref		urx_anchor;

void urx_resolve(rhdtyp *rtn, lab_tabent *lbl_tab, lab_tabent *lbl_top)
{
	urx_rtnref	*rp0, *rp1;
	urx_labref	*lp0, *lp1;
	urx_addr	*ap;

	if (!urx_getrtn(rtn->routine_name.addr, rtn->routine_name.len, &rp0, &rp1, &urx_anchor))
		return;
	while ((ap = rp1->addr) != 0)
	{
		assert(0 == *ap->addr);
#ifdef VMS
		*ap->addr = (int4)rtn->linkage_ptr;
#else
		*ap->addr = (UINTPTR_T)rtn;
#endif
		rp1->addr = ap->next;
		free(ap);
	}
	while (lbl_tab < lbl_top)
	{
		if (urx_getlab(lbl_tab->lab_name.addr, lbl_tab->lab_name.len, rp1, &lp0, &lp1))
		{
			while (0 != (ap = lp1->addr))	/* note the assignment! */
			{
				assert(0 == *ap->addr);
				*ap->addr =
					USHBIN_ONLY((INTPTR_T)&lbl_tab->lnr_adr)
					/* on non-shared binary resolve this address by adding the offset stored at lbl_tab address
					 * to the routine header, to arrive at the address at which the current line number entry is
					 * stored
					 */
					NON_USHBIN_ONLY((INTPTR_T)&lbl_tab->lab_ln_ptr);
				lp1->addr = ap->next;
				free(ap);
			}
			assert(0 == lp1->addr);
			if (lp0 == (urx_labref *)rp1)
				((urx_rtnref *)lp0)->lab = lp1->next;
			else
				lp0->next = lp1->next;
			free(lp1);
		}
		lbl_tab++;
	}
	if (0 == rp1->lab)
	{
		rp0->next = rp1->next;
		free(rp1);
	}
}
