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

#include "mdef.h"

#include "gtm_inet.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"

#include <sys/sem.h>
#include <sys/mman.h>
#include <errno.h>

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtmrecv.h"
#include "iosp.h"
#include "gtm_stdio.h"
#include "gtmio.h"
#include "gtm_string.h"
#include "repl_instance.h"
#include "gtm_logicals.h"
#include "gtm_c_stack_trace.h"
#include "eintr_wrappers.h"
#include "eintr_wrapper_semop.h"
#include "do_semop.h"
#include "ipcrmid.h"
#include "ftok_sems.h"
#include "repl_sem.h"

/* In the present shape, this module is not generic enough. Coded with the view of bringing SEM related code out of
   replication module. Could be made more generic.
   Handles two semaphore sets one for source and another for receiver server.
   FTOK related semaphore for replication is also added here.
   */

GBLREF	jnlpool_addrs		jnlpool;
GBLREF	recvpool_addrs		recvpool;
GBLREF	boolean_t		holds_sem[NUM_SEM_SETS][NUM_SRC_SEMS];

static struct	sembuf	sop[5];
static		int	sem_set_id[NUM_SEM_SETS] = {0, 0};
static		int	sem_info_id_map[SEM_NUM_INFOS] = { GETVAL, GETPID };


boolean_t sem_set_exists(int which_set)
{
	return(sem_set_id[which_set] > 0);
}

int init_sem_set_source(sem_key_t key, int nsems, permissions_t sem_flags)
{
	assert(IPC_PRIVATE == (key_t)key);
	assert(SIZEOF(key_t) >= SIZEOF(sem_key_t));
	sem_set_id[SOURCE] = semget(key, nsems, sem_flags);
	holds_sem[SOURCE][JNL_POOL_ACCESS_SEM] = FALSE;
	holds_sem[SOURCE][SRC_SERV_COUNT_SEM] = FALSE;
	return sem_set_id[SOURCE];
}
void set_sem_set_src(int semid)
{
	sem_set_id[SOURCE] = semid;
	holds_sem[SOURCE][JNL_POOL_ACCESS_SEM] = FALSE;
	holds_sem[SOURCE][SRC_SERV_COUNT_SEM] = FALSE;
}
int init_sem_set_recvr(sem_key_t key, int nsems, permissions_t sem_flags)
{
	int		semid;

	assert(IPC_PRIVATE == (key_t)key);
	assert(SIZEOF(key_t) >= SIZEOF(sem_key_t));
	semid = semget(key, nsems, sem_flags);
	set_sem_set_recvr(semid);
	return sem_set_id[RECV];
}
void set_sem_set_recvr(int semid)
{
	sem_set_id[RECV] = semid;
	holds_sem[RECV][RECV_POOL_ACCESS_SEM] = FALSE;
	holds_sem[RECV][RECV_SERV_COUNT_SEM] = FALSE;
	holds_sem[RECV][UPD_PROC_COUNT_SEM] = FALSE;
	holds_sem[RECV][RECV_SERV_OPTIONS_SEM] = FALSE;
}

int grab_sem(int set_index, int sem_num)
{
	int rc;

	assert(NUM_SRC_SEMS == NUM_RECV_SEMS);	/* holds_sem[][] relies on this as it uses NUM_SRC_SEMS in the array definition */
	assert(!holds_sem[set_index][sem_num]);
	ASSERT_SET_INDEX;
	sop[0].sem_op  = 0; /* Wait for 0 */
	sop[0].sem_num = sem_num;
	sop[1].sem_op  = 1; /* Increment it */
	sop[1].sem_num = sem_num;
	sop[0].sem_flg = sop[1].sem_flg = SEM_UNDO;
	SEMOP(sem_set_id[set_index], sop, 2, rc, FORCED_WAIT);
	if (0 == rc)
		holds_sem[set_index][sem_num] = TRUE;
	return rc;
}

int incr_sem(int set_index, int sem_num)
{
	int rc;

	ASSERT_SET_INDEX;
	sop[0].sem_op  = 1; /* Increment it */
	sop[0].sem_num = sem_num;
	sop[0].sem_flg = SEM_UNDO;
	SEMOP(sem_set_id[set_index], sop, 1, rc, NO_WAIT);
	if (0 == rc)
	{
		/* decr_sem internally calls rel_sem directly which expects that the entry for hold_sem[SOURCE][SRC_SERV_COUNT_SEM]
		 * is set to TRUE. But, incr_sem doesn't set this entry. This causes decr_sem (done below) to assert fail. to avoid
		 * the assert, set the hold_sem array to TRUE even if this is an increment operation
		 */
		holds_sem[set_index][sem_num] = TRUE;
	}
	return rc;
}

int grab_sem_all_source()
{
	int rc;

	assert(!holds_sem[SOURCE][JNL_POOL_ACCESS_SEM]);
	sop[0].sem_op  = 0; /* Wait for 0 */
	sop[0].sem_num = JNL_POOL_ACCESS_SEM;
	sop[1].sem_op  = 1; /* Increment it */
	sop[1].sem_num = JNL_POOL_ACCESS_SEM;
	sop[2].sem_op  = 1; /* Increment it */
	sop[2].sem_num = SRC_SERV_COUNT_SEM;
	sop[0].sem_flg = sop[1].sem_flg = sop[2].sem_flg = SEM_UNDO;
	SEMOP(sem_set_id[SOURCE], sop, 3, rc, FORCED_WAIT);
	if (0 == rc)
	{
		holds_sem[SOURCE][JNL_POOL_ACCESS_SEM] = TRUE;
		holds_sem[SOURCE][SRC_SERV_COUNT_SEM] = TRUE;
	}
	return rc;
}

int grab_sem_all_receive()
{
	int rc;

	assert(!holds_sem[RECV][RECV_POOL_ACCESS_SEM]);
	sop[0].sem_op  = 0; /* Wait for 0 */
	sop[0].sem_num = RECV_POOL_ACCESS_SEM;
	sop[1].sem_op  = 1; /* Increment it */
	sop[1].sem_num = RECV_POOL_ACCESS_SEM;
	sop[2].sem_op  = 1; /* Increment it */
	sop[2].sem_num = RECV_SERV_COUNT_SEM;
	sop[3].sem_op  = 1; /* Increment it */
	sop[3].sem_num = UPD_PROC_COUNT_SEM;
	sop[4].sem_op  = 1; /* Increment it */
	sop[4].sem_num = RECV_SERV_OPTIONS_SEM;
	sop[0].sem_flg = sop[1].sem_flg = sop[2].sem_flg = sop[3].sem_flg = sop[4].sem_flg = SEM_UNDO;
	SEMOP(sem_set_id[RECV], sop, 5, rc, FORCED_WAIT);
	if (0 == rc)
	{
		holds_sem[RECV][RECV_POOL_ACCESS_SEM] = TRUE;
		holds_sem[RECV][RECV_SERV_COUNT_SEM] = TRUE;
		holds_sem[RECV][UPD_PROC_COUNT_SEM] = TRUE;
		holds_sem[RECV][RECV_SERV_OPTIONS_SEM] = TRUE;
	}
	return rc;
}

int grab_sem_immediate(int set_index, int sem_num)
{
	int rc;

	ASSERT_SET_INDEX;
	assert(!holds_sem[set_index][sem_num]);
	sop[0].sem_op  = 0; /* Wait for 0 */
	sop[0].sem_num = sem_num;
	sop[1].sem_op  = 1; /* Increment it */
	sop[1].sem_num = sem_num;
	sop[0].sem_flg = sop[1].sem_flg = SEM_UNDO | IPC_NOWAIT;
	SEMOP(sem_set_id[set_index], sop, 2, rc, NO_WAIT);
	if (0 == rc)
		holds_sem[set_index][sem_num] = TRUE;
	return rc;
}

int rel_sem(int set_index, int sem_num)
{
	int	rc;

	ASSERT_SET_INDEX;
	assert(holds_sem[set_index][sem_num]);
	rc = do_semop(sem_set_id[set_index], sem_num, -1, SEM_UNDO);
	if (0 == rc)
		holds_sem[set_index][sem_num] = FALSE;
	return rc;
}

int decr_sem(int set_index, int sem_num)
{
	return rel_sem(set_index, sem_num);
}

int rel_sem_immediate(int set_index, int sem_num)
{
	int	rc;

	ASSERT_SET_INDEX;
	assert(holds_sem[set_index][sem_num]);
	rc = do_semop(sem_set_id[set_index], sem_num, -1, SEM_UNDO | IPC_NOWAIT);
	if (0 == rc)
		holds_sem[set_index][sem_num] = FALSE;
	return rc;
}

int get_sem_info(int set_index, int sem_num, sem_info_type info_id)
/* Cannot be used to get info which require an additional argument. See man semctl */
{
	ASSERT_SET_INDEX;
	return(semctl(sem_set_id[set_index], sem_num, sem_info_id_map[info_id]));
}

int remove_sem_set(int set_index)
{
	int rc, i;

	ASSERT_SET_INDEX;
	rc = sem_rmid(sem_set_id[set_index]);
	if (!rc) /* successful removal of sem set */
	{
		sem_set_id[set_index] = 0;
		for (i = 0; i < NUM_SRC_SEMS; i++)
			holds_sem[set_index][i] = FALSE;
	}
	return rc;
}
