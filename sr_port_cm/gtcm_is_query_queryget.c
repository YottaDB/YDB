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

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gtcm_protocol.h"
#include "gtcm_is_query_queryget.h"

boolean_t gtcm_is_query_queryget(protocol_msg *peer, protocol_msg *me)
{ /* returns TRUE if QUERY is QUERYGET based on the client/server protocol levels */
	return (0 <= memcmp(peer->msg + CM_LEVEL_OFFSET, CMM_QUERYGET_MIN_LEVEL, 3) &&
	        0 <= memcmp(me->msg + CM_LEVEL_OFFSET, CMM_QUERYGET_MIN_LEVEL, 3));
}
