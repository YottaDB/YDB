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

#include "mdef.h"

#include <ssdef.h>
#include <prtdef.h>
#include <secdef.h>
#include <psldef.h>
#include <syidef.h>
#include <descrip.h>
#include <lkidef.h>
#include <lckdef.h>
#include <efndef.h>

#include "gtm_string.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "repl_sem.h"
#include "vmsdtype.h"
#include "repl_sp.h"
#include "locks.h"
#include "min_max.h"

/* In the present shape, this module is not generic enough. Coded with the
 * view of brining SEM related code out of replication module. Could be made
 * more generic. Handles two semaphore sets one for source and another for
 * receiver server */

#define SEMS_PER_SET		(MAX(NUM_SRC_SEMS, NUM_RECV_SEMS))
#define MAX_LOCKS_QUED		20	/* Max. number of locks for a resource. Required for get_sem_pid() */
#define BUFF_SIZE		255
#define descrcpy(A, B)		(A)->dsc$b_dtype = (B)->dsc$b_dtype;\
				(A)->dsc$b_class = (B)->dsc$b_class;\
				(A)->dsc$w_length= (B)->dsc$w_length;\
				memcpy((A)->dsc$a_pointer, (B)->dsc$a_pointer, (B)->dsc$w_length);
#define INITIAL_N_LOCKS 	8	/* Used in get_lkpid() */

GBLDEF	int4	repl_sem_errno;
GBLDEF	char	repl_msg_buff[BUFF_SIZE];
GBLDEF	mstr    repl_msg_str = {BUFF_SIZE, repl_msg_buff};

typedef struct
{
	char	  res_name[MAX_NAME_LEN + 1];
	sem_key_t key;
	int4	  lock_id;
} sem_ctl_t;

typedef struct
{
	short int       buffer_length;
	short int       item_code;
	void            *bufaddress;
	void            *retaddress;
} t_lki_item_list;

#ifndef __NEW_STARLET
typedef struct lkidef LKIDEF;
#endif

static sem_ctl_t	sem_ctl[NUM_SEM_SETS][SEMS_PER_SET];

boolean_t sem_set_exists(int which_set)
{
	return (0 < sem_ctl[which_set][0].lock_id);
}

int init_sem_set_source(sem_key_t *key)
{
	int status;
	int i, j;

	/* Resource Name format : refer to global_name.c
	 * Example - GT$P_ALPHA2$DKA300$B522D2000000
	 *	prefix -  4 chars = GT$P
	 *	Dev-Id - 14 chars = _ALPHA2$DKA300
	 *	Delimit-  1 char  = $
	 *	File-Id- 12 chars = B522D2000000
	 *      Total    31 chars (Max. length allowed for lock and gsec names!)
	 * Since the length is limited and maximum is already reached, to get unique names for sems in the sem-set,
	 * modify the prefix: GT$P to GT$K, GT$L, and GT$M
	 */

	/* Fill in the resource names */
	for (i = JNL_POOL_ACCESS_SEM; i < NUM_SRC_SEMS; i++)
	{
		sem_ctl[SOURCE][i].key.dsc$a_pointer = sem_ctl[SOURCE][i].res_name;
		descrcpy(&sem_ctl[SOURCE][i].key, key );
	}

	sem_ctl[SOURCE][JNL_POOL_ACCESS_SEM].res_name[3] = 'K';		/* K - Journal Pool Access */
	sem_ctl[SOURCE][SRC_SERV_COUNT_SEM].res_name[3] = 'L';		/* L - Source Server Count */
	sem_ctl[SOURCE][SRC_SERV_OPTIONS_SEM].res_name[3] = 'M';	/* M - Source Server Options */

	/* Keep all the SEMs(locks) in NULL mode */
	for (i = JNL_POOL_ACCESS_SEM; i < NUM_SRC_SEMS; i++)
		if (status = grab_null_lock(SOURCE, i))
			break;
	if (status)
	{
		for (j = JNL_POOL_ACCESS_SEM; j < i; j++) /* grab_null_lock() on i-th sem failed */
		{
			if (SS$_NORMAL != gtm_deq(sem_ctl[SOURCE][j].lock_id, NULL, PSL$C_USER, 0))
			{
				assert(FALSE);
				break;
			}
			sem_ctl[SOURCE][j].lock_id = 0;
		}
	}
	return status;
}

int init_sem_set_recvr(sem_key_t *key)
{
	int status;
	int  i, j;

	/* Fill in the resource names */
	/* Refer to init_sem_set_source() for an explanation */

	for (i = RECV_POOL_ACCESS_SEM; i < NUM_RECV_SEMS; i++)
	{
		sem_ctl[RECV][i].key.dsc$a_pointer = sem_ctl[RECV][i].res_name;
		descrcpy(&sem_ctl[RECV][i].key, key );
	}
	sem_ctl[RECV][RECV_POOL_ACCESS_SEM].res_name[3] = 'T';		/* T - Recv. Pool Access */
	sem_ctl[RECV][RECV_SERV_COUNT_SEM].res_name[3] = 'U';		/* U - Recv. Server Count */
	sem_ctl[RECV][UPD_PROC_COUNT_SEM].res_name[3] = 'V';		/* V - Update Process Count */
	sem_ctl[RECV][RECV_SERV_OPTIONS_SEM].res_name[3] = 'W';		/* W - Recv. Server Options */

	/* Keep all the SEMs in NULL mode */
	for (i = RECV_POOL_ACCESS_SEM; i < NUM_RECV_SEMS; i++)
		if (status = grab_null_lock(RECV, i))
			break;
	if (status)
	{
		for (j = RECV_POOL_ACCESS_SEM; j < i; j++) /* grab_null_lock() on i-th sem failed */
		{
			if (SS$_NORMAL != gtm_deq(sem_ctl[RECV][j].lock_id, NULL, PSL$C_USER, 0))
			{
				assert(FALSE);
				break;
			}
			sem_ctl[RECV][j].lock_id = 0;
		}
	}
	return status;
}

int grab_sem(int set_index, int sem_num)
{
	vms_lock_sb	lksb;
	REPL_SEM_ENQW(set_index, sem_num, LCK$K_EXMODE, LCK$M_CONVERT | LCK$M_NODLCKWT);
	return REPL_SEM_STATUS;
}

int grab_sem_immediate(int set_index, int sem_num)
{
	vms_lock_sb	lksb;
	REPL_SEM_ENQW(set_index, sem_num, LCK$K_EXMODE, LCK$M_CONVERT | LCK$M_NODLCKWT | LCK$M_NOQUEUE);
	return REPL_SEM_STATUS;
}

int rel_sem(int set_index, int sem_num)
{
	vms_lock_sb	lksb;
	REPL_SEM_ENQW(set_index, sem_num, LCK$K_NLMODE, LCK$M_CONVERT | LCK$M_NODLCKWT);
	return REPL_SEM_STATUS;
}

int rel_sem_immediate(int set_index, int sem_num)
{
	vms_lock_sb	lksb;
	REPL_SEM_ENQW(set_index, sem_num, LCK$K_NLMODE, LCK$M_CONVERT | LCK$M_NODLCKWT | LCK$M_NOQUEUE);
	return REPL_SEM_STATUS;
}

int remove_sem_set(int set_index)
{
	int i;

	ASSERT_SET_INDEX;
	if (SOURCE == set_index)
	{
		for (i = JNL_POOL_ACCESS_SEM; i < NUM_SRC_SEMS; i++)
		{
			if (SS$_NORMAL != (repl_sem_errno = gtm_deq(sem_ctl[SOURCE][i].lock_id, NULL, PSL$C_USER, 0)))
			{
				assert(FALSE);
				break;
			}
			sem_ctl[SOURCE][i].lock_id = 0;
		}
	}
	else /* RECIEVER */
	{
		for (i = RECV_POOL_ACCESS_SEM; i < NUM_SRC_SEMS; i++)
		{
			if (SS$_NORMAL != (repl_sem_errno = gtm_deq(sem_ctl[RECV][i].lock_id, NULL, PSL$C_USER, 0)))
			{
				assert(FALSE);
				break;
			}
			sem_ctl[RECV][i].lock_id = 0;
		}
	}
	return REPL_SEM_STATUS;
}

int get_sem_info(int set_index, int sem_num, sem_info_type info_id)
{
	int 	ret_val;
	uint4	status, lk_pid, get_lkpid(struct dsc$descriptor_s *, int, uint4 *);

	ASSERT_SET_INDEX;
	switch(info_id)
	{
		case SEM_INFO_VAL :
			status = get_lkpid(&sem_ctl[set_index][sem_num].key, LCK$K_EXMODE, &lk_pid);
			if (SS$_NORMAL != status)
			{
				repl_sem_errno = status;
				ret_val = -1;
			} else if (lk_pid)
				ret_val = 1;
			else
				ret_val = 0;
			break;
		default :
			ret_val = -1;
			break;
	}
	return ret_val;
}

static int grab_null_lock(int set_index, int sem_num)
{
	vms_lock_sb	lksb;

	repl_sem_errno = gtm_enqw(EFN$C_ENF, LCK$K_NLMODE, &lksb, LCK$M_SYSTEM | LCK$M_EXPEDITE, &sem_ctl[set_index][sem_num].key,
				  0, NULL, 0, NULL, PSL$C_USER, 0);
	assert(SS$_NORMAL == repl_sem_errno);
	if (SS$_NORMAL == repl_sem_errno)
		repl_sem_errno = lksb.cond;
	assert(SS$_NORMAL == repl_sem_errno);
	if (SS$_NORMAL == repl_sem_errno)
		sem_ctl[set_index][sem_num].lock_id = lksb.lockid;
	return REPL_SEM_STATUS;
}

/* check if any process holds the lock identified by name_dsc in a mode at or above given lkmode.
 * If so, return the pid of such a process thru third arg. If there are multiple processes satisfying
 * the criterion, pid of any one such processes is returned. If there is no such process, zero is
 * returned in third arg.
 */

uint4 get_lkpid(struct dsc$descriptor_s *name_dsc, int lkmode, uint4 *lk_pid)
{
	static char	*lki_locks = NULL;
	static int	size = INITIAL_N_LOCKS;
	uint4		ret_len, status;
	vms_lock_sb	lksb;
	LKIDEF		*item, *lki_top;
	t_lki_item_list	lki_item_list[2] = {
				{0, LKI$_LOCKS, 0, &ret_len},
				{0, 0, 0, 0}
			};

	*lk_pid = 0;
	/* Grab Null lock on the resource */
	if (SS$_NORMAL != (status = gtm_enqw(EFN$C_ENF, LCK$K_NLMODE, &lksb, LCK$M_SYSTEM | LCK$M_EXPEDITE,
							name_dsc, 0, NULL, 0, NULL, PSL$C_USER, 0)))
	{
		assert(FALSE);
		return status;
	}
	/* Get a list of all the locks on the resource of interest */
	do
	{
		if (NULL == lki_locks)
			lki_locks = (char *)malloc(size * LKI$C_LENGTH); /* see lkidef.h for LKI$C_LENGTH */
		lki_item_list[0].buffer_length = size * LKI$C_LENGTH;
		lki_item_list[0].bufaddress = lki_locks;
		if (SS$_NORMAL != (status = gtm_getlkiw(EFN$C_ENF, &lksb.lockid, lki_item_list, 0, 0, 0, 0)))
		{
			gtm_deq(lksb.lockid, NULL, PSL$C_USER, 0); /* return status not as important as gtm_getlkiw()'s */
			return status;
		}
		if (!(ret_len >> 31)) /* size of user buffer passed to gtm_getlkiw() was enough */
			break;
		free(lki_locks);
		lki_locks = NULL;
		size <<= 1;
	} while(TRUE);
	/* Release Null lock on the resource (grabbed above) */
	if (SS$_NORMAL != (status = gtm_deq(lksb.lockid, NULL, PSL$C_USER, 0)))
	{
		assert(FALSE);
		return status;
	}
	/* Check through the Locks list got above, if anyone holds the lock in mode >= lkmode.
		If so fill lk_pid with the pid of that process */
	lki_top	= (LKIDEF *)(lki_locks + (ret_len & 0x0000FFFF));
	/* On vax sizeof LKIDEF is not the same as LKI$C_LENGTH (56 vs 24), hence the kludgery below */
	for (item = (LKIDEF *)lki_locks; item < lki_top; item = (LKIDEF *) ((char *)item + LKI$C_LENGTH))
		if (LKI$C_GRANTED == item->lki$b_queue && lkmode <= item->lki$b_grmode)
		{
			*lk_pid = item->lki$l_pid;
			return SS$_NORMAL;
		}
	return SS$_NORMAL; /* Nobody holds the lock in mode >= lkmode */
}
