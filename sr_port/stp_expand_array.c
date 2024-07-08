/****************************************************************
 *								*
 * Copyright (c) 2001-2024 Fidelity National Information	*
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
GBLREF mstr		**stp_array;
GBLREF uint4		stp_array_size;
GBLREF size_t		totalRallocGta;

error_def(ERR_SYSCALL);

void stp_expand_array(void)
{
	mstr		**a;
	uint4		n;
	int		save_errno;

	n = stp_array_size;
	stp_array_size += STP_MAXITEMS;
	assert(stp_array_size > n);
	a = stp_array;
	retry_if_expansion_fails = FALSE;	/* this memory request comes in the middle of stp_gcol, so is doomed if it fails */
	stp_array = (mstr **)stp_mmap(stp_array_size * SIZEOF(mstr *));
	memcpy((uchar_ptr_t)stp_array, (uchar_ptr_t)a, n * SIZEOF(mstr *));
	if (-1 == munmap(a, n * SIZEOF(mstr *)))
	{
		save_errno = errno;
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("munmap()"), CALLFROM, save_errno);
	}
	totalRallocGta -= (n * SIZEOF(mstr *));
	return;
}
