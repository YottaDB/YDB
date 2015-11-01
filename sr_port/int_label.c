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

#include "compiler.h"
#include "toktyp.h"

GBLREF char 	window_token;
GBLREF mval 	window_mval;
GBLREF mident 	window_ident;

void int_label(void)
{
	int len;

	window_token = TK_IDENT;
	len = window_mval.str.len;
	len = (len < MAX_MIDENT_LEN) ? len: MAX_MIDENT_LEN;
	memcpy(window_ident.addr, window_mval.str.addr, len);
	window_ident.len = len;
}
