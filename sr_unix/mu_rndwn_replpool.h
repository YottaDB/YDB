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
int mu_replpool_grab_sem(repl_inst_hdr_ptr_t repl_inst_filehdr, char pool_type, boolean_t *sem_created);
int mu_replpool_remove_sem(repl_inst_hdr_ptr_t repl_inst_filehdr, char pool_type, boolean_t remove_sem);

#endif /* MU_RNDWN_REPLPOOL_INCLUDED */
