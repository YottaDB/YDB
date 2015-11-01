/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
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
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"

#define EIGHT_BLKS_FREE 255

GBLREF sgmnt_data *cs_data;

void bmm_init(void)
{
	assert(MASTER_MAP_SIZE < 32767);	/* or must use longset for VAX */
	memset(cs_data->master_map, EIGHT_BLKS_FREE, MASTER_MAP_SIZE);
	return;
}
