/****************************************************************
 *								*
 * Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2020-2024 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
 *	SORTS_AFTER.C
 *
 */

#include "mdef.h"

#include "sorts_after.h"

/* See comment before SORTS_AFTER macro for function interface */
long	sorts_after(mval *lhs, mval *rhs)
{
	long		result;

	SORTS_AFTER(lhs, rhs, result);
	return result;
}
