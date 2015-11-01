/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_logicals.h"
#include "ztrap_new_init.h"
#include "logical_truth_value.h"

GBLREF	boolean_t	ztrap_new;

void ztrap_new_init(void)
{
	mstr		val;

	val.addr = ZTRAP_NEW;
	val.len = sizeof(ZTRAP_NEW) - 1;
	ztrap_new = logical_truth_value(&val, NULL);
}
