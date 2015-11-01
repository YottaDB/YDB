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

#ifndef __GTCMTR_LKE_SHOW_H__
#define __GTCMTR_LKE_SHOW_H__

bool	gtcmtr_lke_showreq(struct CLB *lnk, char rnum, bool all, bool wait, int4 pid, mstr *node);
char	gtcmtr_lke_showrep(struct CLB *lnk, show_request *sreq);

#endif
