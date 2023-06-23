/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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

void int_label(void)
{
	int len;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	TREF(window_token) = TK_IDENT;
	len = (TREF(window_mval)).str.len;
	len = (len < MAX_MIDENT_LEN) ? len: MAX_MIDENT_LEN;
	memcpy((TREF(window_ident)).addr, (TREF(window_mval)).str.addr, len);
	(TREF(window_ident)).len = len;
}
