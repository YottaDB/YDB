/****************************************************************
 *								*
 * Copyright (c) 2026 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef XPELERRORPRE_INCLUDED
#define XPELERRORPRE_INCLUDED

 #include "util.h"
 error_def(ERR_TEXT); /* BYPASSOK */

static inline void xpelerrorpre()
{
       DCL_THREADGBL_ACCESS;

       SETUP_THREADGBL_ACCESS;
       if (TREF(dollar_zinxpel_roll))
       {
               rts_error(VARLSTCNT(4) ERR_TEXT, 2, RTS_ERROR_TEXT( /* BYPASSOK */
                       "This error originates from the trestart xpel parameter GT.M is executing"));
                util_out_print("", FLUSH);
       }
}
#endif /* XPELERRORPRE_INCLUDED */
