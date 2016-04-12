/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "compiler.h"

/* Create reference to given triple and return constructed reference operator type */
oprtype put_tref(triple *x)
{
	oprtype a;
	a.oprclass = TRIP_REF;
	a.oprval.tref = x;
	return a;
}
