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
#ifndef GTCM_ERR_COMPAT_INCLUDED
#define GTCM_ERR_COMPAT_INCLUDED

/* returns TRUE if rts_error scheme b/n client and server are compatible */
boolean_t gtcm_err_compat(protocol_msg *peer, protocol_msg *me);

#endif /* GTCM_ERR_COMPAT_INCLUDED */
