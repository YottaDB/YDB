/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include <rtnhdr.h>

GBLDEF mval	fgn_label = DEFINE_MVAL_STRING(MV_STR, 0, 0, 0, NULL, 0, 0);

USHBIN_ONLY(lnr_tabent **) NON_USHBIN_ONLY(lnr_tabent *) fgn_resolve_lab(rhdtyp *rtn, int lab_len, char* lab_name)
{
	fgn_label.str.len = lab_len;
	fgn_label.str.addr = lab_name;
	return op_labaddr(rtn, &fgn_label, 0);
}
