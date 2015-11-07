/****************************************************************
 *								*
 *	Copyright 2011, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef GTM_SEMUTILS_H
#define GTM_SEMUTILS_H

#include <sys/sem.h>

/* Database startup wait related macros */
#define DEFAULT_DBINIT_MAX_HRTBT_DELTA	12
#define NO_SEMWAIT_ON_EAGAIN		0
#define INDEFINITE_WAIT_ON_EAGAIN	(uint4) -1
#define MAX_BYPASS_WAIT_SEC		3

#define MAX_C_STACK_TRACES_FOR_SEMWAIT	2

#define DB_CONTROL_SEM	0
#define DB_COUNTER_SEM	1

error_def(ERR_CRITSEMFAIL);
error_def(ERR_DBFILERR);
error_def(ERR_FTOKERR);
error_def(ERR_MAXSEMGETRETRY);
error_def(ERR_SEMKEYINUSE);
error_def(ERR_SEMWT2LONG);
error_def(ERR_SYSCALL);

/* Possible semaphore functions that can fail */
enum sem_syscalls
{
	op_invalid_sem_syscall,
	op_ftok,
	op_semget,
	op_semop,
	op_semctl,
	op_semctl_or_semop
};

enum gtm_semtype
{
	gtm_ftok_sem,
	gtm_access_sem
};

typedef struct semwait_status_struct
{
	int			line_no;
	int			save_errno; /* This value must be checked/assigned only errors. May not be 0 on success. */
	int			status1;
	int			status2;
	int			sem_pid;
	const char		*module;
	enum sem_syscalls	op;
} semwait_status_t;

boolean_t do_blocking_semop(int semid, enum gtm_semtype semtype, uint4 start_hrtbt_cntr,
				semwait_status_t *status, gd_region *reg, boolean_t *bypass);

#define SENDMSG_SEMOP_SUCCESS_IF_NEEDED(STACKTRACE_ISSUED, SEMTYPE)								 \
{																 \
	if (TREF(gtm_environment_init) && STACKTRACE_ISSUED)									 \
	{															 \
		const char		*lcl_msgstr = NULL;									 \
																 \
		lcl_msgstr = (gtm_ftok_sem == SEMTYPE) ? "SEMWT2LONG_FTOK_SUCCEEDED: semop for the ftok semaphore succeeded"	 \
							: "SEMWT2LONG_ACCSEM_SUCCEEDED: semop for the ftok semaphore succeeded"; \
		send_msg(VARLSTCNT(4) ERR_TEXT, 2, LEN_AND_STR(lcl_msgstr));							 \
	}															 \
}

#define DBFILERR_PARAMS(REG)			ERR_DBFILERR, 2, DB_LEN_STR(REG)
#define CRITSEMFAIL_PARAMS(REG)			ERR_CRITSEMFAIL,2, DB_LEN_STR(REG)
#define SYSCALL_PARAMS(RETSTAT, OP)		ERR_SYSCALL, 5, LEN_AND_STR(OP), LEN_AND_STR(RETSTAT->module), RETSTAT->line_no
#define SEMKEYINUSE_PARAMS(UDI)			ERR_SEMKEYINUSE, 1, UDI->key
#define SEMWT2LONG_PARAMS(REG, RETSTAT, GTM_SEMTYPE, TOT_WAIT_TIME)						\
						ERR_SEMWT2LONG, 7, process_id, TOT_WAIT_TIME,			\
							LEN_AND_LIT(GTM_SEMTYPE), DB_LEN_STR(REG), RETSTAT->sem_pid

#define GET_OP_STR(RETSTAT, OP)												\
{															\
	switch (RETSTAT->op)												\
	{														\
		case op_semget:												\
			OP = "semget()";										\
			break;												\
		case op_semop:												\
			OP = "semop()";											\
			break;												\
		case op_semctl:												\
			OP = "semctl()";										\
			break;												\
		case op_semctl_or_semop:										\
			OP = "semctl()/semop()";									\
			break;												\
		default:												\
			OP = "";											\
			assert(FALSE);											\
	}														\
}

#define ISSUE_SEMWAIT_ERROR(RETSTAT, REG, UDI, GTM_SEMTYPE)							\
{														\
	const char	*op;											\
	uint4		tot_wait_time;										\
														\
	GBLREF uint4	process_id;										\
														\
	if (ERR_CRITSEMFAIL == RETSTAT->status2)								\
	{													\
		if (0 == RETSTAT->status1)									\
		{												\
				GET_OP_STR(RETSTAT, op);							\
				rts_error(VARLSTCNT(16) DBFILERR_PARAMS(REG), CRITSEMFAIL_PARAMS(REG), 		\
					SYSCALL_PARAMS(RETSTAT, op), RETSTAT->save_errno);			\
		} else if (ERR_SEMKEYINUSE == RETSTAT->status1)							\
		{												\
			rts_error(VARLSTCNT(11) DBFILERR_PARAMS(REG), CRITSEMFAIL_PARAMS(REG),			\
				SEMKEYINUSE_PARAMS(UDI));							\
		} else												\
			GTMASSERT;										\
	} else if (ERR_MAXSEMGETRETRY == RETSTAT->status2)							\
	{													\
		rts_error(VARLSTCNT(7) DBFILERR_PARAMS(REG), ERR_MAXSEMGETRETRY, 1, MAX_SEMGET_RETRIES);	\
	} else if (ERR_FTOKERR == RETSTAT->status2)								\
	{													\
		rts_error(VARLSTCNT(9) DBFILERR_PARAMS(REG), ERR_FTOKERR, 2, DB_LEN_STR(REG), 			\
				RETSTAT->save_errno);								\
	} else if (0 == RETSTAT->status2)									\
	{													\
		assert(ERR_SEMWT2LONG == RETSTAT->status1);							\
		assert(RETSTAT->sem_pid && (-1 != RETSTAT->sem_pid));						\
		tot_wait_time = TREF(dbinit_max_hrtbt_delta) * HEARTBEAT_INTERVAL_IN_SECS;			\
		rts_error(VARLSTCNT(13) DBFILERR_PARAMS(REG),							\
			SEMWT2LONG_PARAMS(REG, RETSTAT, GTM_SEMTYPE, tot_wait_time));				\
	} else													\
		GTMASSERT;											\
}

/* Set the value of semaphore number 2 ( = FTOK_SEM_PER_ID - 1) as GTM_ID. This way, in case of an orphaned
 * semaphore (say, kill -9), MUPIP RUNDOWN will be able to identify GT.M semaphore from the value and will
 * remove it.
 */
#define SET_GTM_ID_SEM(SEMID, RC)											\
{															\
	union semun		semarg;											\
															\
	semarg.val = GTM_ID;												\
	RC = semctl(SEMID, FTOK_SEM_PER_ID - 1, SETVAL, semarg);							\
}

/* Set up typical GT.M semaphore (access control semaphore and/or ftok semaphore) */
#define SET_GTM_SOP_ARRAY(SOP, SOPCNT, INCR_CNT, SEMFLG)									\
{																\
	/* Typically, multiple statements are not specified in a single line. However, each of the 2 lines below represent	\
	 * "one" semaphore operation and hence an acceptible exception to the coding guidelines.				\
	 */															\
	SOP[0].sem_num = DB_CONTROL_SEM; SOP[0].sem_op = 0;	/* Wait for 0 (unlocked) */					\
	SOP[1].sem_num = DB_CONTROL_SEM; SOP[1].sem_op = 1;	/* Then lock it */						\
	if (INCR_CNT)														\
	{															\
		SOP[2].sem_num = DB_COUNTER_SEM; SOP[2].sem_op = 1;	/* Increment counter semaphore */			\
		SOPCNT = 3;													\
	} else															\
		SOPCNT = 2;													\
	SOP[0].sem_flg = SOP[1].sem_flg = SOP[2].sem_flg = SEMFLG;								\
}

#define RETURN_SEMWAIT_FAILURE(RETSTAT, ERRNO, OP, STATUS1, STATUS2, SEMPID)						\
{															\
	RETSTAT->line_no = __LINE__;											\
	RETSTAT->save_errno = ERRNO;											\
	RETSTAT->op = OP;												\
	RETSTAT->status1 = STATUS1;											\
	RETSTAT->status2 = STATUS2;											\
	RETSTAT->sem_pid = SEMPID;											\
	RETSTAT->module = __FILE__;											\
	return FALSE;													\
}

#define SEM_REMOVED(ERRNO)	((EINVAL == ERRNO) || (EIDRM == ERRNO))	/* EIDRM is only on Linux */

#endif /* GTM_SEMUTILS_H */
