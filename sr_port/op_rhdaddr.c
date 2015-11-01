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
#include "rtnhdr.h"
#include "op.h"

GBLREF mident zlink_mname;

rhdtyp	*op_rhdaddr(mval *name, rhdtyp *rhd)
{
	mval	routine;
	mident	routname;
	rhdtyp	*answer;
	error_def	(ERR_ZLINKFILE);
	error_def	(ERR_ZLMODULE);

	if (rhd != 0)
	{
		answer = rhd;
	}
	else
	{
		MV_FORCE_STR(name);
		routine = *name;
		routine.str.len = (routine.str.len > sizeof(mident) ? sizeof(mident) : routine.str.len);
		memcpy(&routname, routine.str.addr, routine.str.len);
		routine.str.addr = (char *)&routname;
		if ((answer = find_rtn_hdr(&routine.str)) == 0)
		{
			op_zlink (&routine, 0);
			answer = find_rtn_hdr(&routine.str);
			if (answer == 0)
				rts_error(VARLSTCNT(8) ERR_ZLINKFILE, 2, name->str.len, name->str.addr,
					ERR_ZLMODULE, 2, mid_len (&zlink_mname), &zlink_mname);
#if	defined (__alpha) && defined (__vms)
			answer = answer->linkage_ptr;
			if (answer == 0)
				rts_error(VARLSTCNT(8) ERR_ZLINKFILE, 2, name->str.len, name->str.addr,
					ERR_ZLMODULE, 2, mid_len (&zlink_mname), zlink_mname.c);
#endif
		}
	}
	return answer;
}
