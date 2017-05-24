/****************************************************************
 *								*
 * Copyright (c) 2012-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stdarg.h>
#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "op.h"
#include "stringpool.h"
#include "gvn2gds.h"

GBLREF spdesc 	stringpool;

/*
 * -----------------------------------------------
 * op_fnzcollate()
 * Converts between global variable name (GVN) and database internal representation (GDS) using
 * a specified collation sequence.
 *
 * Arguments:
 *	src	     - Pointer to a string in Global Variable Name format, if reverse is 0, or
 *		       a pointer to a string in database internal format if reverse is nonzero.
 * 	col	     - Collation algorithm index.
 * 	reverse	     - Specifies whether to convert from GVN to GDS representation (0) or whether
 * 		       to convert from GDS to GVN representation (nonzero).
 * 	dst	     - The destination string containing the converted string.
 * -----------------------------------------------
 */
void op_fnzcollate(mval *src, int col, int reverse, mval *dst)
{
	gv_key			save_currkey[DBKEYALLOC(MAX_KEY_SZ)];
	gv_key			*gvkey;
	unsigned char		*key;
	unsigned char		buff[MAX_ZWR_KEY_SZ];
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_STR(src);
	if (0 == reverse)
	{	/* gvn2gds */
		gvkey = &save_currkey[0];
		key = gvn2gds(src, gvkey, col);
		/* If input has error at some point, copy whatever subscripts
		 * (+ gblname) have been successfully parsed */
		COPY_ARG_TO_STRINGPOOL(dst, key, &gvkey->base[0]);
	} else
	{	/* reverse: gds2gvn */
		key = gds2gvn(src, &buff[0], col);
		COPY_ARG_TO_STRINGPOOL(dst, key, &buff[0]);
	}
}

