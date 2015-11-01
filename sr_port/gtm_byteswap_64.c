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

qw_num gtm_byteswap_64(qw_num num64)
{
#ifndef INT8_SUPPORTED
	qw_num		swap_qw;
	uint32_t	swap_uint32;

	QWASSIGN(swap_qw, num64);
	swap_uint32 = GTM_BYTESWAP_32(swap_qw.value[lsb_index]);
	swap_qw.value[lsb_index] = GTM_BYTESWAP_32(swap_qw.value[msb_index]);
	swap_qw.value[msb_index] = swap_uint32;
	return (swap_qw);
#else
	GTMASSERT; /* should use GTM_BYTESWAP_64 macro, not gtm_byteswap_64 function */
	return 0;
#endif
}
