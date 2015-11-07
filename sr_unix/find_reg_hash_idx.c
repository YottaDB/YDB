/****************************************************************
 *								*
 *	Copyright 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "muextr.h"

GBLDEF	gd_addr		*gd_header;

/* This routine finds the position of a region in the global directory which we use to index arrays of region information  */
int find_reg_hash_idx(gd_region *reg)
{
	gd_region *regl;
	int index;

	for (index = gd_header->n_regions-1, regl = gd_header->regions + index; reg != regl; regl--, index--)
		assertpro(0 <= index);
	return index;
}
