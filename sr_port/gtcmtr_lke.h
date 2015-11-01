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

#ifndef GTCMTR_LKE_INCLUDED
#define GTCMTR_LKE_INCLUDED

bool gtcmtr_lke_clearreq(struct CLB *lnk, char rnum, bool all, bool interactive,
     int4 pid, mstr *node);
bool gtcmtr_lke_showreq(struct CLB *lnk, char rnum, bool all, bool wait, int4 pid, mstr *node);

#endif /* GTCMTR_LKE_INCLUDED */
