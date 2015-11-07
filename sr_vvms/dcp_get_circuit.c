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

#include "ddphdr.h"
#include "ddpcom.h"
#include "decddp.h"
#include "trans_log_name.h"
#include "getzprocess.h"
#include "five_bit.h"
#include "is_five_bit.h"

GBLDEF unsigned short	my_circuit = 0;
GBLDEF mstr		my_circuit_name;
static unsigned char	cktnam_buff[MAX_TRANS_NAME_LEN];

condition_code dcp_get_circuit(mval *logical)
{
	condition_code	status;

	error_def(ERR_DDPINVCKT);

	status = trans_log_name(&logical->str, &my_circuit_name, cktnam_buff);
	if (SS$_NORMAL == status)
	{
		if (DDP_CIRCUIT_NAME_LEN == my_circuit_name.len && is_five_bit(my_circuit_name.addr))
			my_circuit = five_bit(my_circuit_name.addr);
		else
			status = ERR_DDPINVCKT;
	}
	return status;
}
