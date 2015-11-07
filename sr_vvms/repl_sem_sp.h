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

#ifndef _REPL_SEM_SP_H
#define _REPL_SEM_SP_H

#define REPL_SEM_ERRNO		repl_sem_errno
#define REPL_SEM_ERROR		(gtm_getmsg(repl_sem_errno, &repl_msg_str),repl_msg_buff)
#define REPL_SEM_NOT_GRABBED	(SS$_NOTQUEUED == repl_sem_errno)
#define REPL_SEM_NOT_GRABBED1	(SS$_NOTQUEUED == save_errno)
#define REPL_STR_ERROR		(gtm_getmsg(status, &repl_msg_str),repl_msg_buff)
#define REPL_STR_ERROR1(status) (gtm_getmsg(status, &repl_msg_str),repl_msg_buff)
#define REPL_SEM_STATUS		((SS$_NORMAL == repl_sem_errno) ? 0 : -1)

#define REPL_SEM_ENQW(set_index, sem_num, mode, flags)									\
	ASSERT_SET_INDEX;												\
	lksb.lockid = sem_ctl[set_index][sem_num].lock_id; 								\
	repl_sem_errno = gtm_enqw(EFN$C_ENF, mode, &lksb, flags, NULL, 0, NULL, 0, NULL, PSL$C_USER, 0);		\
	assert(SS$_NORMAL == repl_sem_errno || (((flags) & LCK$M_NOQUEUE) && (SS$_NOTQUEUED == repl_sem_errno)));	\
	if (SS$_NORMAL == repl_sem_errno) 										\
		repl_sem_errno = lksb.cond;										\
	assert(SS$_NORMAL == repl_sem_errno || (((flags) & LCK$M_NOQUEUE) && (SS$_NOTQUEUED == repl_sem_errno)));	\

typedef struct dsc$descriptor_s sem_key_t;

int init_sem_set_source(sem_key_t *key);
int init_sem_set_recvr(sem_key_t *key);

GBLREF int4 repl_sem_errno;
GBLREF char repl_msg_buff[];
GBLREF mstr repl_msg_str;
#endif /* _REPL_SEM_SP_H */
