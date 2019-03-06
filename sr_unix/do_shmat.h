/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef DO_SHMAT_H_INCLUDED
#define DO_SHMAT_H_INCLUDED

void *do_shmat(int4 shmid, const void *shm_base, int4 shmflg);
void *do_shmat_exec_perm(int4 shmid, size_t shm_size, int *save_errno);

#endif
