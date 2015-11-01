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

#ifndef _REPL_SEM_SP_H
#define _REPL_SEM_SP_H

#define REPL_SEM_ERRNO		errno
#define REPL_SEM_ERROR		STRERROR(errno)
#define REPL_SEM_NOT_GRABBED	(EAGAIN == errno)
#define REPL_SEM_NOT_GRABBED1	(EAGAIN == save_errno)
#define REPL_STR_ERROR		STRERROR(errno)

typedef int sem_key_t;
typedef int permissions_t;

int init_sem_set_source(sem_key_t key, int nsems, permissions_t sem_flags);
int init_sem_set_recvr(sem_key_t key, int nsems, permissions_t sem_flags);
#endif /* _REPL_SEM_SP_H */
