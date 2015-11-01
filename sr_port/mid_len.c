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

int mid_len(mident *name)
{

	register int fast;

	for (fast=0; fast < sizeof(mident) && name->c[fast]; fast++)
		;
	return fast;

}
