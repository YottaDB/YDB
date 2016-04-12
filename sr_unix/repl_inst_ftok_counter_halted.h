/****************************************************************
 *								*
 * Copyright (c) 2016 Fidelity National Information		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef REPL_INST_FTOK_COUNTER_HALTED_INCLUDED
#define REPL_INST_FTOK_COUNTER_HALTED_INCLUDED

#define	CHECK_IF_REPL_INST_FTOK_COUNTER_HALTED(INST_HDR, UDI, COUNTER_HALTED, HALTED_BY_ME, SETUP_ERROR, POOLREG, SAVE_ERRNO)	\
{																\
	if (INST_HDR.ftok_counter_halted)											\
	{	/* If counter is halted, we should never remove ftok semaphore as part of a later "ftok_sem_release" call.	\
		 * So make sure "counter_ftok_incremented" is set to FALSE in case it was TRUE after the "ftok_sem_get" call.	\
		 */														\
		UDI->counter_ftok_incremented = FALSE;										\
		HALTED_BY_ME = FALSE;												\
	} else if (COUNTER_HALTED)												\
	{															\
		assert(!UDI->counter_ftok_incremented);										\
		HALTED_BY_ME = TRUE;												\
		if (!INST_HDR.qdbrundown)											\
		{														\
			assert(ERANGE == SAVE_ERRNO);										\
			ftok_sem_release(POOLREG, FALSE, TRUE);									\
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) SETUP_ERROR, 0, ERR_TEXT, 2,					\
				RTS_ERROR_LITERAL("Error incrementing the ftok semaphore counter"), SAVE_ERRNO);		\
		}														\
	} else															\
	{															\
		HALTED_BY_ME = FALSE;												\
		assert(UDI->counter_ftok_incremented);										\
	}															\
}

void	repl_inst_ftok_counter_halted(unix_db_info *udi, char *file_type, repl_inst_hdr *repl_instance);

#endif /* REPL_INST_FTOK_COUNTER_HALTED_INCLUDED */
