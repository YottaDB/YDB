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
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "spec_type.h"
#include "get_spec.h"

GBLREF gv_key *gv_altkey;
GBLREF gv_namehead *gv_target;
static readonly int	spec_len[LAST_TYPE_DEFINED]={4,4};

uchar_ptr_t get_spec(uchar_ptr_t spec_rec_addr, int spec_rec_len, unsigned char spec_type)
{
	uchar_ptr_t	ptr, top;
	error_def(ERR_GVIS);
	error_def(ERR_INVSPECREC);

	for (ptr = spec_rec_addr, top = ptr + spec_rec_len; ptr < top;  ptr += spec_len[*ptr])
	{
		if (*ptr == spec_type)
			return ptr;
		else
			if (*ptr > LAST_TYPE_DEFINED)
			{
				gv_target->root = 0;
				rts_error(VARLSTCNT(6) ERR_INVSPECREC, 0, ERR_GVIS, 2, gv_altkey->end - 1, gv_altkey->base);
			}
	}
	return (uchar_ptr_t)0;
}
