/****************************************************************
 *								*
 *	Copyright 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef _REPL_SEM_H
#define _REPL_SEM_H

#include "repl_sem_sp.h"

#define ASSERT_SET_INDEX	assert (NUM_SEM_SETS > set_index)

enum
{
	SOURCE,
	RECV,
	NUM_SEM_SETS
};

typedef enum
{
	JNL_POOL_ACCESS_SEM,	/* For Startup / Shutdown / Options-change */
	SRC_SERV_COUNT_SEM,	/* Source sever holds it while alive */
	DUMMY_SEM1,		/* added just to make the number of semaphores same as in recvpool */
	DUMMY_SEM2,		/* added just to make the number of semaphores same as in recvpool */
	SOURCE_ID_SEM,
	NUM_SRC_SEMS
} source_sem_type;

/* The need for the options semaphore is documented towards the end of Section 6.2.6 of the design spec for C9F06-002729 */
typedef enum
{
	RECV_POOL_ACCESS_SEM,	/* For Startup / Shutdown */
	RECV_SERV_COUNT_SEM,	/* Receiver sever holds it while alive */
	UPD_PROC_COUNT_SEM,	/* Update process holds it while alive */
	RECV_SERV_OPTIONS_SEM,	/* For options change, since it is done through the shared memory */
	RECV_ID_SEM,
	NUM_RECV_SEMS
} recv_sem_type;

typedef enum
{
	SEM_INFO_VAL,
	SEM_INFO_PID,
	SEM_NUM_INFOS
} sem_info_type;

int		grab_sem(int set_index, int sem_num);	/* set_index can be SOURCE or RECV  */
int		grab_sem_immediate(int set_index, int sem_num);
int		rel_sem(int set_index, int sem_num);
int		rel_sem_immediate(int set_index, int sem_num);
int		get_sem_info(int set_index, int sem_num, sem_info_type info_id);
int		remove_sem_set(int set_index);
boolean_t	sem_set_exists(int which_set);

#ifdef UNIX
void 		rel_recvpool_ftok_sems(boolean_t, boolean_t);
void 		rel_jnlpool_ftok_sems(boolean_t, boolean_t);
void 		lock_recvpool_ftok_sems(boolean_t, boolean_t);
void 		lock_jnlpool_ftok_sems(boolean_t, boolean_t);
void 		get_lock_recvpool_ftok_sems(boolean_t, boolean_t);
void 		get_lock_jnlpool_ftok_sems(boolean_t, boolean_t);
void 		set_sem_set_src(int semid);
void 		set_sem_set_recvr(int semid);
int 		grab_sem_all_source(void);	/* rollback needs this */
int 		grab_sem_all_receive(void);	/* rollback needs this */
int		incr_sem(int set_index, int sem_num);
int		decr_sem(int set_index, int sem_num);
#endif

#endif /* _REPL_SEM_H */
