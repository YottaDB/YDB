/****************************************************************
 *								*
 *	Copyright 2005, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MU_ALL_VERSION_STANDALONE_H_INCLUDED
#define MU_ALL_VERSION_STANDALONE_H_INCLUDED

/* When the sem_info structure below is allocated, it must be allocated in an array of
   at least the dimension of FTOK_ID_CNT.
*/
#define FTOK_ID_CNT 3

/* Structure that holds information for semaphores we create for standalone processing */
typedef struct
{
	int		ftok_key;
	int		sem_id;
} sem_info;

void mu_all_version_get_standalone(char_ptr_t db_fn, sem_info *sem_inf);
void mu_all_version_release_standalone(sem_info *sem_id);

#endif
