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

#ifndef MU_RNDWN_ALL_INCLUDED
#define MU_RNDWN_ALL_INCLUDED

int mu_rndwn_all(void);
int mu_rndwn_sem_all(void);
int parse_sem_id(char *);
boolean_t is_orphaned_gtm_semaphore(int);

#endif /* MU_RNDWN_ALL_INCLUDED */
