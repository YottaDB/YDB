/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef HAVE_CRIT_H_INCLUDED
#define HAVE_CRIT_H_INCLUDED

/* states of CRIT passed as argument to have_crit() */
#define CRIT_IN_COMMIT		0x00000001
#define CRIT_NOT_TRANS_REG	0x00000002
#define CRIT_RELEASE		0x00000004
#define CRIT_ALL_REGIONS	0x00000008

/* Note absence of any flags is default value which finds if any region
   or the replication pool have crit or are getting crit. It returns
   when one is found without checking further.
*/
#define CRIT_HAVE_ANY_REG	0x00000000

uint4 have_crit(uint4 crit_state);

#endif /* HAVE_CRIT_H_INCLUDED */
