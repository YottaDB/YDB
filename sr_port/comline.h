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

#define MAX_RECALL			99
#define	MAX_RECALL_NUMBER_LENGTH	2	/* i.e. maximum recallable strings 99 */
#define clmod(x)	((x + MAX_RECALL) % MAX_RECALL)
