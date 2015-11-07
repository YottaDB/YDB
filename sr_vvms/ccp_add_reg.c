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
#include "ccp.h"

GBLREF ccp_db_header *ccp_reg_root;

void ccp_add_reg(d)
ccp_db_header *d;
{
	/* see note for ccp_get_reg...go ordered list for efficiency */
	d->next = ccp_reg_root;
	ccp_reg_root = d;
	return;
}
