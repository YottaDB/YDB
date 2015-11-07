/****************************************************************
 *								*
 *	Copyright 2013 Fidelity Information Services, Inc	*
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
#include "gvnh_spanreg.h"

/* Check if "reg" is one of the regions that the global corresponding to "gvnh_reg" maps to in the gld file pointed to by "addr" */
boolean_t gvnh_spanreg_ismapped(gvnh_reg_t *gvnh_reg, gd_addr *addr, gd_region *reg)
{
	gvnh_spanreg_t	*gvspan;
	gd_binding	*map, *map_top;

	gvspan = gvnh_reg->gvspan;
	assert(NULL != gvspan);
	assert(gvspan->start_map_index > 0);
	assert(gvspan->end_map_index > 0);
	assert(gvspan->start_map_index < addr->n_maps);
	assert(gvspan->end_map_index < addr->n_maps);
	map = &addr->maps[gvspan->start_map_index];
	map_top = &addr->maps[gvspan->end_map_index];
	assert(map < map_top);
	for ( ; map <= map_top; map++)
		if (reg == map->reg.addr)
			return TRUE;
	return FALSE;
}
