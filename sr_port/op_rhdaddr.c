/****************************************************************
 *								*
 *	Copyright 2001, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include <rtnhdr.h>
#include "op.h"

GBLREF mident_fixed	zlink_mname;
GBLREF rtn_tabent	*rtn_names;

#ifdef USHBIN_SUPPORTED
LITDEF mval		literal_null;
#endif

error_def(ERR_ZLINKFILE);
error_def(ERR_ZLMODULE);

/* For routine name given, return routine header address if rhd not already set */
rhdtyp	*op_rhdaddr(mval *name, rhdtyp *rhd)
{
#	ifdef USHBIN_SUPPORTED
	return op_rhd_ext(name, (mval *)&literal_null, rhd, NULL);
#	else
	return (NULL != rhd) ? rhd : op_rhdaddr1(name);
#	endif
}

/* Find the newest linked version of a routine */
rhdtyp	*op_rhdaddr1(mval *name)
{
	mval		routine;
	mident_fixed	routname;
	rhdtyp		*answer;

	MV_FORCE_STR(name);
	routine = *name;
	routine.str.len = (MAX_MIDENT_LEN < routine.str.len ? MAX_MIDENT_LEN : routine.str.len);
	memcpy(&routname.c[0], routine.str.addr, routine.str.len);
	routine.str.addr = (char *)&routname.c[0];
	if ((NULL == rtn_names) || (NULL == (answer = find_rtn_hdr(&routine.str))))	/* Note assignment */
	{	/* Initial check for rtn_names is so we avoid the call to find_rtn_hdr() if we have just
		 * unlinked all modules as find_rtn_hdr() does not deal well with an empty rtn table.
		 */
		op_zlink(&routine, NULL);
		answer = find_rtn_hdr(&routine.str);
		if (NULL == answer)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_ZLINKFILE, 2, name->str.len, name->str.addr,
				ERR_ZLMODULE, 2, strlen(&zlink_mname.c[0]), &zlink_mname);
#		if defined (__alpha) && defined (__vms)
		answer = answer->linkage_ptr;
		if (NULL == answer)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_ZLINKFILE, 2, name->str.len, name->str.addr,
				ERR_ZLMODULE, 2, strlen(&zlink_mname.c[0]), zlink_mname.c);
#		endif
	}
#	ifdef USHBIN_SUPPORTED
	/* In this (autorelink) context, no need to pass 4th arg (*lnr) since other opcodes used in conjunction with
	 * op_rhdaddr1 will handle label offset if necessary.
	 */
	return op_rhd_ext(&routine, (mval *)&literal_null, answer, NULL);
#	else
	/* Non-autorelink context just returns the routine header address */
	return answer;
#	endif
}
