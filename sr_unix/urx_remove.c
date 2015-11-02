/****************************************************************
 *								*
 *	Copyright 2002, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"
#include <rtnhdr.h>
#include "urx.h"

/* Routine to run the unresolved chain in search of entries that point into the linkage section that
   is about to be released (for USHBIN type platforms) or for unresolves that point into the code
   section for non-USHBIN platforms (currently only used by triggers on non-USHBIN platforms). The
   The boundaries of the pertinent section is computed appropriately from the passed in routine header.
   Located entries are removed from the unresolved chains (routine and label chains).
*/

GBLREF urx_rtnref	urx_anchor;

#define IS_IN_RANGE(start, end, item) ((char *)(item) >= (char *)(start) && (char *)(item) < (char *)(end))

void urx_remove(rhdtyp *rtnhdr)
{
	urx_rtnref	*rtn, *rtnprev;
	urx_labref	*lab, *labprev;
	urx_addr	*addr, *addrprev, *savaddr;
	char		*regstart, *regend;
	int		deletes;

	DEBUG_ONLY(deletes = 0);
#ifdef USHBIN_SUPPORTED
	/* All unresolved addresses will point into the linkage section */
	regstart = (char *)rtnhdr->linkage_adr;
	regend = regstart + (SIZEOF(lnk_tabent) * rtnhdr->linkage_len);
#else
	/* All unresolved addresses will point into the code section */
	regstart = PTEXT_ADR(rtnhdr);
	regend = PTEXT_END_ADR(rtnhdr);
#endif
	rtnprev = &urx_anchor;
	rtn = rtnprev->next;
	while (rtn)
	{	/* For each unresolved routine.. */
		addrprev = NULL;
		addr = rtn->addr;
		while (addr)
		{	/* Run list of resolve addrs for this routine */
			if (IS_IN_RANGE(regstart, regend, addr->addr))
			{	/* We will be deleting an element so addrprev will not be changing */
				if (NULL == addrprev)
					rtn->addr = addr->next;		/* First element being removed */
				else
					addrprev->next = addr->next;
				savaddr = addr->next;
				free(addr);
				addr = savaddr;
				DEBUG_ONLY(++deletes);
				continue;
			}
			addrprev = addr;
			addr = addr->next;
		}

		/* Note that the structure of the urx_labref and urx_rtnref is critical here. The urx_rtnref serves
		   as an anchor for the urx_labref chain by virtue of urx_rtnref's "lab" pointer being at the same offset
		   as urx_labref's "next" pointer.
		*/
		labprev = (urx_labref *)rtn;
		lab = rtn->lab;
		while (lab)
		{
			addrprev = NULL;
			addr = lab->addr;
			while (addr)
			{
				if (IS_IN_RANGE(regstart, regend, addr->addr))
				{	/* We will be deleting an element so addrprev will not be changing */
					if (NULL == addrprev)
						lab->addr = addr->next;		/* First element being removed */
					else
						addrprev->next = addr->next;
					savaddr = addr->next;
					free(addr);
					addr = savaddr;
					DEBUG_ONLY(++deletes);
					continue;
				}
				addrprev = addr;
				addr = addr->next;
			}
			if (NULL == lab->addr)
			{	/* No references to this label left .. remove from unresolved chain */
				labprev->next = lab->next;
				free(lab);
				lab = labprev->next;
				DEBUG_ONLY(++deletes);
				continue;
			}
			labprev = lab;
			lab = lab->next;
		}

		/* Note that it is possible to have a routine on the unresolved chain with no addr chain of unresolves
		   for it yet there are labels unresolved. This would be the case if a routine contained a call to a
		   non-existent label. It is not an error until/unless the call call is executed. The reverse is also
		   true, it is possible to have an unresolved addr chain for the routine with no labels. This occurs
		   when a call using indirection such as DO @LBL^RTN. In this case, there will be no unresolved label
		   but there will be an unresolved routine.
		*/
		if (NULL == rtn->addr && NULL == rtn->lab)
		{	/* This node has no reason to keep living */
			rtnprev->next = rtn->next;
			free(rtn);
			rtn = rtnprev->next;
			DEBUG_ONLY(++deletes);
			continue;
		}
		rtnprev = rtn;
		rtn = rtn->next;
	}
#ifdef DEBUG_URX
	PRINTF("urx_remove: Deleted %d entries\n", deletes);
#endif
}
