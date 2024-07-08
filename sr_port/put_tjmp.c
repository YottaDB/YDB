/****************************************************************
 *								*
 * Copyright (c) 2001-2024 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "compiler.h"

oprtype put_tjmp(triple *x)
{
<<<<<<< HEAD
	assert(NULL != x);
	oprtype a;
||||||| parent of 07a11a26bb (GT.M V7.1-004)
oprtype a;
a.oprclass = TJMP_REF;
a.oprval.tref = x;
return a;
=======
	oprtype a;

>>>>>>> 07a11a26bb (GT.M V7.1-004)
	a.oprclass = TJMP_REF;
	a.oprval.tref = x;
	return a;
}
