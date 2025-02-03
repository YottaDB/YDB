/****************************************************************
 *								*
 * Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2019-2024 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "is_equ.h"

/* args:
 *	u, v		- pointers to mvals
 *
 */
boolean_t is_equ(mval *u, mval *v)
{
	boolean_t	result;

	IS_EQU(u, v, result);
	return result;
}
