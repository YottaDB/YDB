/****************************************************************
 *								*
 *	Copyright 2001, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __DO_SHMAT_H__
#define __DO_SHMAT_H__

void *do_shmat(int4 shmid, const void *shm_base, int4 shmflg);
void *do_shmat_exec_perm(int4 shmid, size_t shm_size, int *save_errno);

#endif
