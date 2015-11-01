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
#ifndef GTCM_QUERY_IS_QUERYGET_H_INCLUDED
#define GTCM_QUERY_IS_QUERYGET_H_INCLUDED

/* returns TRUE if QUERY is QUERYGET based on the client/server protocol levels */
boolean_t gtcm_is_query_queryget(protocol_msg *peer, protocol_msg *me);

#endif /* GTCM_QUERY_IS_QUERYGET_H_INCLUDED */
