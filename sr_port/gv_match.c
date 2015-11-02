/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "dpgbldir.h"
#include "gv_match.h"

/* Multiple regions from across multiple global directories might correspond to the same physical file.
 * This routine detects if a given input region's database file matches with the corresponding file of an already open region.
 * If yes it returns the FIRST matching region else it returns NULL.
 */
gd_region *gv_match(gd_region *reg)
{
	gd_region	*r_top, *gv_region;
	gd_addr		*addr_ptr;

	for (addr_ptr = get_next_gdr(NULL); addr_ptr; addr_ptr = get_next_gdr(addr_ptr))
	{
		for (gv_region = addr_ptr->regions, r_top = gv_region + addr_ptr->n_regions; gv_region < r_top; gv_region++)
		{
			if (!gv_region->open)
				continue;
			if (REG_EQUAL(FILE_INFO(reg), gv_region))
				return gv_region;
		}
	}
	return NULL;
}
