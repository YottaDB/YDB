/****************************************************************
*                                                              *
*      Copyright 2011 Fidelity Information Services, Inc *
*                                                              *
*      This source code contains the intellectual property     *
*      of its copyright holder(s), and is made available       *
*      under a license.  If you do not know the terms of       *
*      the license, please stop and do not read further.      *
*                                                              *
****************************************************************/

#ifndef GTM_C_STACK_TRACE_SEMOP_INCLUDED
#define GTM_C_STACK_TRACE_SEMOP_INCLUDED
#include <sys/sem.h>
#include "semwt2long_handler.h"

#ifdef DEBUG
#include "wbox_test_init.h"
#define CHECK_SEMVAL_GRT_SEMOP(SEMID, SEMNUM, SEM_OP)								\
{														\
	int sems_val;												\
	if (0 > (SEM_OP))											\
	{													\
		sems_val = semctl(SEMID, SEMNUM, GETVAL);							\
		if (-1 != sems_val)										\
			assert ((sems_val >= abs(SEM_OP)) ||(gtm_white_box_test_case_enabled && 		\
					(WBTEST_MUR_ABNORMAL_EXIT_EXPECTED == gtm_white_box_test_case_number)));\
	}													\
}
#else
#define CHECK_SEMVAL_GRT_SEMOP(SEMID, SEMNUM, SEMOP) {}
#endif

#define MAX_SEM_WAIT_TIME (1000 * 60) /* 60 seconds */
int try_semop_get_c_stack(int semid, struct sembuf sops[], int nsops);

#endif /*GTM_C_STACK_TRACE_SEMOP_INCLUDED*/
