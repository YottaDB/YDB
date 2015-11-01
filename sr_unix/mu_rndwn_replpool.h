/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MU_RNDWN_REPLPOOL_INCLUDED
#define MU_RNDWN_REPLPOOL_INCLUDED

boolean_t mu_rndwn_replpool(replpool_identifier *replpool_id, int sem_id, int shmid);
boolean_t mu_replpool_grab_sem(boolean_t immediate);
boolean_t mu_replpool_remove_sem(boolean_t immediate);

#endif /* MU_RNDWN_REPLPOOL_INCLUDED */
