/****************************************************************
 *								*
 * Copyright (c) 2001-2024 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2024 YottaDB LLC and/or its subsidiaries.	*
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
||||||| parent of 07a11a26bb (GT.M V7.1-004)
#include "gtm_string.h"
=======
#include "gtm_string.h"
#include "send_msg.h"
>>>>>>> 07a11a26bb (GT.M V7.1-004)

GBLREF boolean_t	retry_if_expansion_fails;
GBLREF mstr		**stp_array;
<<<<<<< HEAD
GBLREF gtm_uint64_t	stp_array_size;
||||||| parent of 07a11a26bb (GT.M V7.1-004)
GBLREF uint4		stp_array_size;
=======
GBLREF uint4		stp_array_size;
GBLREF size_t		totalRallocGta;

error_def(ERR_SYSCALL);
>>>>>>> 07a11a26bb (GT.M V7.1-004)

void stp_expand_array(void)
{
	mstr		**a;
<<<<<<< HEAD
	gtm_uint64_t	n;
||||||| parent of 07a11a26bb (GT.M V7.1-004)
	mstr **a;
	uint4 n;
=======
	uint4		n;
	int		save_errno;
>>>>>>> 07a11a26bb (GT.M V7.1-004)

	n = stp_array_size;
	stp_array_size = ((MAXINT4 > n) ? (n * 2) : (n + MAXINT4));
	a = stp_array;
	retry_if_expansion_fails = FALSE;	/* this memory request comes in the middle of stp_gcol, so is doomed if it fails */
<<<<<<< HEAD
	stp_array = (mstr **)malloc(stp_array_size * SIZEOF(mstr *));
||||||| parent of 07a11a26bb (GT.M V7.1-004)
	stp_array = (mstr **) malloc(stp_array_size * SIZEOF(mstr *));
=======
	stp_array = (mstr **)stp_mmap(stp_array_size * SIZEOF(mstr *));
>>>>>>> 07a11a26bb (GT.M V7.1-004)
	memcpy((uchar_ptr_t)stp_array, (uchar_ptr_t)a, n * SIZEOF(mstr *));
	if (-1 == munmap(a, n * SIZEOF(mstr *)))
	{
		save_errno = errno;
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("munmap()"), CALLFROM, save_errno);
	}
	totalRallocGta -= (n * SIZEOF(mstr *));
	return;
}
