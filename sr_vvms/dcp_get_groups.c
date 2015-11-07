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

#include "gtm_ctype.h"

#include <ssdef.h>
#include <lnmdef.h>

#include "ddphdr.h"
#include "ddpcom.h"
#include "decddp.h"
#include "trans_log_name.h"

GBLREF unsigned short	my_group_mask;
GBLREF mstr		my_circuit_name;

condition_code dcp_get_groups(void)
{
	char		*grpp, *grpp_top, digit;
	char		group_list_buffer[MAX_TRANS_NAME_LEN], group_logical_buffer[MAX_TRANS_NAME_LEN];
	mstr 		group_list, group_logical;
	condition_code	status;
	int 		group;
	unsigned short	group_mask;

	assert(DDP_DEFAULT_GROUP_MASK == my_group_mask); /* this function should be called only once, this is a crude check */
	group_logical.addr = group_logical_buffer;
	memcpy(group_logical.addr, DDP_GROUP_LOGICAL_PREFIX, STR_LIT_LEN(DDP_GROUP_LOGICAL_PREFIX));
	memcpy(&group_logical.addr[STR_LIT_LEN(DDP_GROUP_LOGICAL_PREFIX)], my_circuit_name.addr, my_circuit_name.len);
	group_logical.len = STR_LIT_LEN(DDP_GROUP_LOGICAL_PREFIX) + my_circuit_name.len;
	if (SS$_NORMAL != (status = trans_log_name(&group_logical, &group_list, group_list_buffer)))
	{
		if (SS$_NOLOGNAM == status)
			status = SS$_NORMAL;
		return status;
	}
	for (group = 0, group_mask = 0, grpp = group_list.addr, grpp_top = group_list.addr + group_list.len;
	     grpp < grpp_top;
	     grpp++, group = 0)
	{
		if (',' == *grpp)
			continue;
		do
		{
			digit = *grpp;
			if (ISDIGIT(digit))
				group = (group * 10 + digit - '0');
			else
				break;
		} while (++grpp < grpp_top);
		if (',' == digit || grpp == grpp_top)
		{
			if (DDP_MAX_GROUP > group)
				group_mask |= (1 << group);
		} else
		{ /* skip to the next group */
			while (++grpp < grpp_top && ',' != *grpp)
				;
		}
	}
	my_group_mask = group_mask;
	return SS$_NORMAL;
}
