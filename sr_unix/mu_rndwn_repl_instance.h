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

#ifndef MU_RNDWN_REPL_INSTANCE_INCLUDED
#define MU_RNDWN_REPL_INSTANCE_INCLUDED

boolean_t mu_rndwn_repl_instance(replpool_identifier *replpool_id, boolean_t immediate, boolean_t rndwn_both_pools,
					boolean_t *jnlpool_sem_created);

#endif /* MU_RNDWN_REPL_INSTANCE_INCLUDED */
