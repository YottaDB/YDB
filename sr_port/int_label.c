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

#include "gtm_string.h"

#include "compiler.h"
#include "toktyp.h"

GBLREF char window_token;
GBLREF mval window_mval;
GBLREF mident window_ident;

void int_label(void)
{

	int len,left;

	window_token = TK_IDENT;
	len = window_mval.str.len;
	len = len < sizeof(mident) ? len: sizeof(mident);
	memcpy(window_ident.c, window_mval.str.addr, len);
	left = sizeof(mident) - len;
	memset(window_ident.c + len, 0, left);

}
