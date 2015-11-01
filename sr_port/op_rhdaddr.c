/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "rtnhdr.h"
#include "op.h"

GBLREF mident_fixed zlink_mname;

rhdtyp	*op_rhdaddr(mval *name, rhdtyp *rhd)
{
	mval		routine;
	mident_fixed	routname;
	rhdtyp		*answer;
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
		routine.str.len = (routine.str.len > MAX_MIDENT_LEN ? MAX_MIDENT_LEN : routine.str.len);
		memcpy(&routname.c[0], routine.str.addr, routine.str.len);
		routine.str.addr = (char *)&routname.c[0];
		if ((answer = find_rtn_hdr(&routine.str)) == 0)
		{
			op_zlink (&routine, 0);
			answer = find_rtn_hdr(&routine.str);
			if (answer == 0)
				rts_error(VARLSTCNT(8) ERR_ZLINKFILE, 2, name->str.len, name->str.addr,
					ERR_ZLMODULE, 2, strlen(&zlink_mname.c[0]), &zlink_mname);
#if	defined (__alpha) && defined (__vms)
			answer = answer->linkage_ptr;
			if (answer == 0)
				rts_error(VARLSTCNT(8) ERR_ZLINKFILE, 2, name->str.len, name->str.addr,
					ERR_ZLMODULE, 2, strlen(&zlink_mname.c[0]), zlink_mname.c);
#endif
		}
	}
	return answer;
}
