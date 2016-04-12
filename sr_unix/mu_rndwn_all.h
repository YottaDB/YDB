/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MU_RNDWN_ALL_INCLUDED
#define MU_RNDWN_ALL_INCLUDED

typedef struct semid_queue_elem_t
{
	int4 semid;
	struct semid_queue_elem_t *prev;
} semid_queue_elem;

int mu_rndwn_all(void);
int mu_rndwn_sem_all(void);
int parse_sem_id(char *);
boolean_t is_orphaned_gtm_semaphore(int);
boolean_t in_keep_sems_list(int semid);
void add_to_semids_list(int semid);
#endif /* MU_RNDWN_ALL_INCLUDED */
