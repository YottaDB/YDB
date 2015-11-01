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

static struct	sembuf	sop[5];
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

int grab_sem_all_source()
{
	int rc;

	sop[0].sem_op  = 0; /* Wait for 0 */
	sop[0].sem_num = JNL_POOL_ACCESS_SEM;
	sop[1].sem_op  = 1; /* Increment it */
	sop[1].sem_num = JNL_POOL_ACCESS_SEM;
	sop[2].sem_op  = 1; /* Increment it */
	sop[2].sem_num = SRC_SERV_COUNT_SEM;
	sop[3].sem_op  = 1; /* Increment it */
	sop[3].sem_num = SRC_SERV_OPTIONS_SEM;
	sop[0].sem_flg = sop[1].sem_flg = sop[2].sem_flg = sop[3].sem_flg = SEM_UNDO;
	SEMOP(sem_set_id[SOURCE], sop, 4, rc);
	return rc;
}

int grab_sem_all_receive()
{
	int rc;

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
	SEMOP(sem_set_id[RECV], sop, 5, rc);
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
