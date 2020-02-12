/****************************************************************
 *								*
 * Copyright (c) 2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

#include "mdef.h"

#include "compiler.h"
#include "mval2num.h"

/* Note: Updates numeric portion of mval (if needed) in place.
 *	 So `v` is an input and output parameter.
 * Returns "v" (needed in VIEW "NOUNDEF" mode in case input `v` was undefined and `&literal_null` got copied over to `v`).
 */
mval *mval2num(mval *v)
{
	if (MV_IS_SQLNULL(v))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZYSQLNULLNOTVALID);
	MV_FORCE_NUM(v);	/* this could modify `v` in case `v` was undefined and VIEW "NOUNDEF" is turned on */
	if (MVTYPE_IS_NUM_APPROX(v->mvtype))
		n2s(v);
	return v;	/* used by caller in case `v` got modified in MV_FORCE_NUM above */
}
