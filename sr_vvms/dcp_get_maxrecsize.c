/****************************************************************
 *								*
 *	Copyright 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <ssdef.h>
#include <lnmdef.h>
#include <stddef.h>

#include "ddphdr.h"
#include "ddpcom.h"
#include "decddp.h"
#include "trans_log_name.h"
#include "getzprocess.h"
#include "five_bit.h"
#include "is_five_bit.h"

GBLDEF int4	ddp_max_rec_size;
GBLREF mstr	my_circuit_name;

condition_code dcp_get_maxrecsize(void)
{
	mstr		recsize, recsize_logical;
	char		recbuff[MAX_TRANS_NAME_LEN], recsize_logical_buffer[MAX_TRANS_NAME_LEN];
	condition_code	status;
	error_def(ERR_DDPRECSIZNOTNUM);

	recsize_logical.addr = recsize_logical_buffer;
	memcpy(recsize_logical.addr, DDP_MAXRECSIZE_PREFIX, STR_LIT_LEN(DDP_MAXRECSIZE_PREFIX));
	memcpy(&recsize_logical.addr[STR_LIT_LEN(DDP_MAXRECSIZE_PREFIX)], my_circuit_name.addr, my_circuit_name.len);
	recsize_logical.len = STR_LIT_LEN(DDP_MAXRECSIZE_PREFIX) + my_circuit_name.len;
	if (SS$_NORMAL == (status = trans_log_name(&recsize_logical, &recsize, recbuff)))
	{
		if (-1 != (ddp_max_rec_size = asc2i(recbuff, recsize.len)))
		{
			if (DDP_MIN_RECSIZE > ddp_max_rec_size)
				ddp_max_rec_size = DDP_MIN_RECSIZE;
		} else
			status = ERR_DDPRECSIZNOTNUM;
	}
	return status;
}
