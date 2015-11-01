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

#include "mdef.h"

#include <sys/sem.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
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
#include "eintr_wrappers.h"
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

static struct	sembuf	sop[2];
static		int	sem_set_id[NUM_SEM_SETS] = {0, 0};
static		int	sem_info_id_map[SEM_NUM_INFOS] = { GETVAL, GETPID };


boolean_t sem_set_exists(int which_set)
{
	return(sem_set_id[which_set] > 0);
}

int init_sem_set_source(sem_key_t key, int nsems, permissions_t sem_flags)
{
	sem_set_id[SOURCE] = semget(key, nsems, sem_flags);
	return sem_set_id[SOURCE];
}
void set_sem_set_src(int semid)
{
	sem_set_id[SOURCE] = semid;
}
int init_sem_set_recvr(sem_key_t key, int nsems, permissions_t sem_flags)
{
	sem_set_id[RECV] = semget(key, nsems, sem_flags);
	return sem_set_id[RECV];
}
void set_sem_set_recvr(int semid)
{
	sem_set_id[RECV] = semid;
}


int grab_sem(int set_index, int sem_num)
{
	int rc;

	ASSERT_SET_INDEX;
	sop[0].sem_op  = 0; /* Wait for 0 */
	sop[0].sem_num = sem_num;
	sop[1].sem_op  = 1; /* Increment it */
	sop[1].sem_num = sem_num;
	sop[0].sem_flg = sop[1].sem_flg = SEM_UNDO;
	SEMOP(sem_set_id[set_index], sop, 2, rc);
	return rc;
}

int grab_sem_immediate(int set_index, int sem_num)
{
	int rc;

	ASSERT_SET_INDEX;
	sop[0].sem_op  = 0; /* Wait for 0 */
	sop[0].sem_num = sem_num;
	sop[1].sem_op  = 1; /* Increment it */
	sop[1].sem_num = sem_num;
	sop[0].sem_flg = sop[1].sem_flg = SEM_UNDO | IPC_NOWAIT;
	SEMOP(sem_set_id[set_index], sop, 2, rc);
	return rc;
}

int rel_sem(int set_index, int sem_num)
{
	ASSERT_SET_INDEX;
	return(do_semop(sem_set_id[set_index], sem_num, -1, SEM_UNDO));
}

int rel_sem_immediate(int set_index, int sem_num)
{
	ASSERT_SET_INDEX;
	return(do_semop(sem_set_id[set_index], sem_num, -1, SEM_UNDO | IPC_NOWAIT));
}

int get_sem_info(int set_index, int sem_num, sem_info_type info_id)
/* Cannot be used to get info which require an additional argument. See man semctl */
{
	ASSERT_SET_INDEX;
	return(semctl(sem_set_id[set_index], sem_num, sem_info_id_map[info_id]));
}

int remove_sem_set(int set_index)
{
	int rc;

	ASSERT_SET_INDEX;
	rc = sem_rmid(sem_set_id[set_index]);
	if (!rc) /* successful removal of sem set */
		sem_set_id[set_index] = 0;
	return rc;
}

/* This will create ftok semaphore and then lock jnlpool */
void get_lock_jnlpool_ftok_sems(boolean_t incr_cnt, boolean_t immediate)
{
	error_def(ERR_TEXT);
	error_def(ERR_JNLPOOLSETUP);

	if (!ftok_sem_get(jnlpool.jnlpool_dummy_reg, incr_cnt, JNLPOOL_ID, immediate))
		rts_error(VARLSTCNT(6) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
			RTS_ERROR_LITERAL("Error from ftok_sem_get in get_lock_jnlpool_ftok_sems"));
}

/* This will create ftok semaphore and then lock recvpool */
void get_lock_recvpool_ftok_sems(boolean_t incr_cnt, boolean_t immediate)
{
	error_def(ERR_TEXT);
	error_def(ERR_RECVPOOLSETUP);

	if (!ftok_sem_get(recvpool.recvpool_dummy_reg, incr_cnt, RECVPOOL_ID, immediate))
		rts_error(VARLSTCNT(6) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
			RTS_ERROR_LITERAL("Error from ftok_sem_get in get_lock_recvpool_ftok_sems"));
}

/* This will lock jnlpool using ftok semaphore. We assume semaphore already exists */
void lock_jnlpool_ftok_sems(boolean_t incr_cnt, boolean_t immediate)
{
	error_def(ERR_TEXT);
	error_def(ERR_JNLPOOLSETUP);

	if (!ftok_sem_lock(jnlpool.jnlpool_dummy_reg, incr_cnt, immediate))
		rts_error(VARLSTCNT(6) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
			RTS_ERROR_LITERAL("Error from ftok_sem_lock in lock_jnlpool_ftok_sems"));
}

/* This will lock recvpool using ftok semaphore. We assume semaphore already exists */
void lock_recvpool_ftok_sems(boolean_t incr_cnt, boolean_t immediate)
{
	error_def(ERR_TEXT);
	error_def(ERR_RECVPOOLSETUP);

	if (!ftok_sem_lock(recvpool.recvpool_dummy_reg, incr_cnt, immediate))
		rts_error(VARLSTCNT(6) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
			RTS_ERROR_LITERAL("Error from ftok_sem_lock in lock_recvpool_ftok_sems"));
}

/* release jnlpool lock */
void rel_jnlpool_ftok_sems(boolean_t decr_cnt, boolean_t immediate)
{
	error_def(ERR_TEXT);
	error_def(ERR_JNLPOOLSETUP);

	if (!ftok_sem_release(jnlpool.jnlpool_dummy_reg, decr_cnt, immediate))
		rts_error(VARLSTCNT(6) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
			RTS_ERROR_LITERAL("Error from ftok_sem_release in rel_jnlpool_ftok_lock"));
}

/* release recvpool lock */
void rel_recvpool_ftok_sems(boolean_t decr_cnt, boolean_t immediate)
{
	error_def(ERR_TEXT);
	error_def(ERR_RECVPOOLSETUP);

	if (!ftok_sem_release(recvpool.recvpool_dummy_reg, decr_cnt, immediate))
		rts_error(VARLSTCNT(6) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
			RTS_ERROR_LITERAL("Error from ftok_sem_release in rel_recvpool_ftok_lock"));
}
