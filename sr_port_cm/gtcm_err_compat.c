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
#include "gtcm_err_compat.h"

boolean_t gtcm_err_compat(protocol_msg *peer, protocol_msg *me)
{ /* returns TRUE if rts_error scheme b/n client and server are compatible */
	boolean_t	peer_is_vms, i_am_vms;

	peer_is_vms = (0 == memcmp(peer->msg + CM_OS_OFFSET, "VMS", 3));
#if defined(VMS)
	i_am_vms = TRUE;
	assert(0 == memcmp(me->msg + CM_OS_OFFSET, "VMS", 3));
#elif defined(UNIX)
	i_am_vms = FALSE;
	assert(0 != memcmp(me->msg + CM_OS_OFFSET, "VMS", 3));
#else
#error Unsupported Platform
#endif
	return ((peer_is_vms && i_am_vms) || (!peer_is_vms && !i_am_vms));
}
