/****************************************************************
 *								*
 * Copyright (c) 2001-2026 Fidelity National Information	*
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
#include "stringpool.h"
#include "stp_parms.h"
#include "gtm_string.h"
#include "send_msg.h"

GBLREF boolean_t	retry_if_expansion_fails;
GBLREF size_t		totalRallocGta;

error_def(ERR_SYSCALL);

void stp_expand_array(uint4 *array_size_p, mstr ***array_p, size_t element_size, size_t request)
{
	void	*a;
	uint4				n;
	int				save_errno;

	n = *array_size_p;
	*array_size_p += request ? (ROUND_UP(request, STP_MAXITEMS)) : STP_MAXITEMS;
	assert(*array_size_p > n);
	a = *array_p;
	retry_if_expansion_fails = FALSE;	/* this memory request comes in the middle of stp_gcol, so is doomed if it fails */
	*array_p = (mstr **)malloc(*array_size_p * element_size);
	memcpy((uchar_ptr_t)*array_p, (uchar_ptr_t)a, n * element_size);
	free(a);
	return;
}
