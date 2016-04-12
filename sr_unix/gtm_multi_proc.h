/****************************************************************
 *								*
 * Copyright (c) 2015-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GTM_MULTI_PROC_H
#define GTM_MULTI_PROC_H

typedef	void *(*gtm_multi_proc_fnptr_t)(void *parm);

int	gtm_multi_proc(gtm_multi_proc_fnptr_t fnptr, int ntasks, int max_procs,
					void **ret_array, void *parm_array, int parmElemSize,
					size_t extra_shm_size, gtm_multi_proc_fnptr_t init_fnptr,
					gtm_multi_proc_fnptr_t finish_fnptr);
void	gtm_multi_proc_helper(void);
int	gtm_multi_proc_finish(gtm_multi_proc_fnptr_t finish_fnptr);

GBLREF	boolean_t	multi_proc_in_use;		/* TRUE => parallel processes active ("gtm_multi_proc"). False otherwise */
GBLREF	unsigned char	*multi_proc_key;		/* NULL for parent process; Non-NULL for forked off child processes */
#ifdef DEBUG
GBLREF	boolean_t	multi_proc_key_exception;
#endif

error_def(ERR_MULTIPROCLATCH);
														\
#define	MULTI_PROC_MAX_PROCS		1000	/* We expect max # of tasks to execute to be in the hundreds, not thousands */
#define	MULTI_PROC_LATCH_TIMEOUT_SEC	(4 * 60)        /* Define latch timeout as being 4 mins */

#define	GRAB_MULTI_PROC_LATCH_IF_NEEDED(RELEASE_LATCH)								\
{														\
	GBLREF	uint4	process_id;										\
														\
	if (multi_proc_in_use)											\
	{													\
		RELEASE_LATCH = FALSE;										\
		if (process_id != multi_proc_shm_hdr->multi_proc_latch.u.parts.latch_pid)			\
		{												\
			if (!grab_latch(&multi_proc_shm_hdr->multi_proc_latch, MULTI_PROC_LATCH_TIMEOUT_SEC))	\
			{											\
				assert(FALSE);									\
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4)					\
					ERR_MULTIPROCLATCH, 2, LEN_AND_LIT("GRAB_MULTI_PROC_LATCH_IF_NEEDED"));	\
			}											\
			RELEASE_LATCH = TRUE;									\
		}												\
	}													\
}

#define	REL_MULTI_PROC_LATCH_IF_NEEDED(RELEASE_LATCH)						\
{												\
	GBLREF	uint4	process_id;								\
												\
	if (multi_proc_in_use)									\
	{											\
		assert(process_id == multi_proc_shm_hdr->multi_proc_latch.u.parts.latch_pid);	\
		if (RELEASE_LATCH)								\
			rel_latch(&multi_proc_shm_hdr->multi_proc_latch);			\
	}											\
}

#define	SET_FORCED_MULTI_PROC_EXIT					\
{									\
	if (multi_proc_in_use)						\
		multi_proc_shm_hdr->forced_multi_proc_exit = TRUE;	\
}

#define	IS_FORCED_MULTI_PROC_EXIT(mp_hdr)	(mp_hdr->forced_multi_proc_exit)

/* Structure in shared memory used for inter-process communications amongst the multiple processes forked off by "gtm_multi_proc" */
typedef struct
{
	global_latch_t		multi_proc_latch;	/* latch used to obtain critical section by multiple processes */
	boolean_t		forced_multi_proc_exit;	/* flag to signal multiple processes to exit at a logical point */
	boolean_t		wait_done;	/* WAITPID of all children processes completed by parent */
	int			shmid;		/* id of the shared memory segment created by "gtm_multi_proc" */
	int			procs_created;	/* # of processes that have been forked off by "gtm_multi_proc" */
	int			parent_pid;			/* Store pid of parent process */
	int			next_task;	/* next task available to be picked up by a free process */
	pid_t			pid[MULTI_PROC_MAX_PROCS];	/* Store pid of forked off processes. Cleared when pid dies */
	pid_t			orig_pid[MULTI_PROC_MAX_PROCS];	/* Copy of pid of forked off processes. For debugging purposes. */
	int4			wait_stat[MULTI_PROC_MAX_PROCS];/* Status of WAITPID call. For debugging purposes. */
	/* Below are parameters from "gtm_multi_proc" invocation */
	gtm_multi_proc_fnptr_t	fnptr;
	void			**pvt_ret_array;	/* array of return values passed in by caller (points to private memory) */
	void			**shm_ret_array;	/* array of return values (points to shared memory) */
	void			*parm_array;
	int			ntasks;
	int			max_procs;
	int			parmElemSize;
} multi_proc_shm_hdr_t;

GBLREF	multi_proc_shm_hdr_t	*multi_proc_shm_hdr;	/* Pointer to "multi_proc_shm_hdr_t" structure in shared memory */

#endif
