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

#ifndef CALLG_H
#define CALLG_H

/* defined as macro on MVS */
#ifndef callg
int callg(int(*)(), void *);
#endif
void callg_signal(void *);

#endif
