 /***************************************************************
 *								*
 * Copyright (c) 2022 Fidelity National Information		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"
#include "op.h"
#include "mvalconv.h"
#include "arit.h"
#include "gtm_stdio.h"
#include "gtm_stdlib.h"
#include "gtm_strings.h"
#include "restrict.h"
#include "dm_audit_log.h"
#include "error.h"
#include "errorsp.h"
#include "gtmmsg.h"
#include "send_msg.h"
#include "compiler.h"
#include "gtmimagename.h"

GBLREF  boolean_t               	gtm_dist_ok_to_use, restrict_initialized;
GBLREF  char                    	gtm_dist[GTM_PATH_MAX];
GBLREF  struct restrict_facilities      restrictions;
error_def(ERR_RESTRICTEDOP);

void op_fnzauditlog(mval* src, mval* dst)
{
	boolean_t 	check;
	int		len;
	mval 		input_line;
	DCL_THREADGBL_ACCESS;

        SETUP_THREADGBL_ACCESS;
	MV_FORCE_STR(src);
	input_line.str.addr = src->str.addr;
	input_line.str.len = src->str.len;
	TREF(is_zauditlog) = TRUE;
	check = RESTRICTED(aza_enable);
	if (FALSE == check)
	{
		MV_FORCE_MVAL(dst, 0);
		TREF(is_zauditlog) = FALSE;
		return;
	}
	if (check && !dm_audit_log(&input_line, AUDIT_SRC_GTM))
	{
		MV_FORCE_MVAL(dst, 0);
		TREF(is_zauditlog) = FALSE;
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) MAKE_MSG_WARNING(ERR_RESTRICTEDOP), 1, "$ZAUDITLOG()");
		return;
	}
	TREF(is_zauditlog) = FALSE;
	MV_FORCE_MVAL(dst, 1);
	return;
}
