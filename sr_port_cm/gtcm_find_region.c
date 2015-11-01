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
#include "hashdef.h"
#include "cmidef.h"
#include "cmmdef.h"
#include "gtcm_find_region.h"

cm_region_list *gtcm_find_region(connection_struct *cnx, unsigned char rnum)
{
	error_def(CMERR_REGNTFND);
	cm_region_list *ptr;

	for (ptr = cnx->region_root ; ptr && ptr->regnum != rnum ; ptr = ptr->next)
		;
	if (!ptr)
		rts_error(VARLSTCNT(1) CMERR_REGNTFND);
	return ptr;
}
