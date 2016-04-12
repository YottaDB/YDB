/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "gdsbml.h"


GBLREF sgmnt_data *cs_data;

void bmm_init(void)
{
	assert(cs_data && cs_data->master_map_len);
	memset(MM_ADDR(cs_data), BMP_EIGHT_BLKS_FREE, MASTER_MAP_SIZE(cs_data));
	return;
}
