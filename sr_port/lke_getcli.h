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

#ifndef __LKE_GETCLI_H__
#define __LKE_GETCLI_H__

int4 lke_getcli(bool *all, bool *wait, bool *inta, int4 *pid, mstr *region, mstr *node,
	mstr *one_lock, bool *memory, bool *nocrit);

#ifdef VMS
#define HEXPID
#endif

#endif
