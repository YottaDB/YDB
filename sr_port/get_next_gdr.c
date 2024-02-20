/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2024 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "dpgbldir.h"

GBLREF gd_addr	*gd_addr_head;

/*+
Function:       GET_NEXT_GDR

		This function returns the next entry in the list of open
		global directories.  If the input parameter is zero, the
		first entry is returned, otherwise the next entry in the
		list is returned.  If the input parameter is not a member
		of the list, then zero will be returned.

Syntax:         gd_addr *get_next_gdr(gd_addr *prev)

Prototype:      ?

Return:         *gd_addr -- a pointer to the global directory structure

Arguments:      The previous global directory accessed;

Side Effects:   NONE

Notes:          NONE
-*/
gd_addr *get_next_gdr(gd_addr *prev)
{
	gd_addr	*ptr;

	if (NULL == prev)
		return gd_addr_head;
	return prev->link;
}

