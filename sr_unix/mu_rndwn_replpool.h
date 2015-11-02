/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MU_RNDWN_REPLPOOL_INCLUDED
#define MU_RNDWN_REPLPOOL_INCLUDED

int mu_rndwn_replpool(replpool_identifier *replpool_id, repl_inst_hdr_ptr_t repl_inst_filehdr, int shmid, boolean_t *ipc_rmvd);
int mu_replpool_grab_sem(repl_inst_hdr_ptr_t repl_inst_filehdr, char pool_type, boolean_t *sem_created, boolean_t immediate);
int mu_replpool_release_sem(repl_inst_hdr_ptr_t repl_inst_filehdr, char pool_type, boolean_t remove_sem);

#define MAX_IPCS_ID_BUF		64	/* Shared memory or semaphore ID is an int and so won't be more than 12 digits long */

#define ISSUE_REPLPOOLINST(SAVE_ERRNO, SHM_ID, INSTFILENAME, FAILED_OP)						\
{														\
	gtm_putmsg(VARLSTCNT(5) ERR_REPLPOOLINST, 3, SHM_ID, LEN_AND_STR(INSTFILENAME));			\
	gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT(FAILED_OP), CALLFROM, SAVE_ERRNO);			\
}

#endif /* MU_RNDWN_REPLPOOL_INCLUDED */
