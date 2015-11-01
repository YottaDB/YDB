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

#ifndef GTCMTR_LK_H_INCLUDED
#define GTCMTR_LK_H_INCLUDED

bool gtcmtr_lkacquire(void);
bool gtcmtr_lkcanall(void);
bool gtcmtr_lkcancel(void);
bool gtcmtr_lkdelete(void);
bool gtcmtr_lkreqimmed(void);
bool gtcmtr_lkreqnode(void);
bool gtcmtr_lkrequest(void);
bool gtcmtr_lkresume(void);
bool gtcmtr_lksuspend(void);

#endif
