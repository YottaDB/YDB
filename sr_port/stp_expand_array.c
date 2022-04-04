/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "compiler.h"
#include "stringpool.h"
#include "stp_parms.h"

<<<<<<< HEAD
GBLREF mstr		**stp_array;
GBLREF gtm_uint64_t	stp_array_size;
=======
GBLREF boolean_t	retry_if_expansion_fails;
GBLREF mstr		**stp_array;
GBLREF uint4		stp_array_size;
>>>>>>> eb3ea98c (GT.M V7.0-002)

void stp_expand_array(void)
{
	mstr		**a;
	gtm_uint64_t	n;

	n = stp_array_size;
	stp_array_size = ((MAXINT4 > n) ? (n * 2) : (n + MAXINT4));
	a = stp_array;
<<<<<<< HEAD
	stp_array = (mstr **)malloc(stp_array_size * SIZEOF(mstr *));
=======
	retry_if_expansion_fails = FALSE;	/* this memory request comes in the middle of stp_gcol, so is doomed if it fails */
	stp_array = (mstr **) malloc(stp_array_size * SIZEOF(mstr *));
>>>>>>> eb3ea98c (GT.M V7.0-002)
	memcpy((uchar_ptr_t)stp_array, (uchar_ptr_t)a, n * SIZEOF(mstr *));
	free(a);
	return;
}
