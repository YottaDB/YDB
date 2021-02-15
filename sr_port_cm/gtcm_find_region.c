/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gtcm_find_region.h"

error_def(CMERR_REGNTFND);

cm_region_list *gtcm_find_region(connection_struct *cnx, unsigned char rnum)
{
	if (rnum > cnx->maxregnum || !cnx->region_array[rnum])
		RTS_ERROR_ABT(VARLSTCNT(1) CMERR_REGNTFND);
	else
		return cnx->region_array[rnum];
	return NULL;
}
