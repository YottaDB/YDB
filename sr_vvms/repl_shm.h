/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef _REPL_SHM_H
#define _REPL_SHM_H

/* signoff_from_gsec() invokes gtm_deq() and is used in a lot of places.
 * In DBG mode, we want to assert fail whenever gtm_deq() returns abnormal status.
 * Hence we define a new function that does a call to gtm_deq() and asserts on status.
 * In PRO, we dont want the extra-function-call overhead, hence the macro is a direct call to gtm_deq().
 */
#ifndef DEBUG
#define		signoff_from_gsec(gsec_lockid)		gtm_deq(gsec_lockid, NULL, PSL$C_USER, 0)
#else
#define		signoff_from_gsec(gsec_lockid)		signoff_from_gsec_dbg(gsec_lockid)
#endif

#define		detach_shm(shm_range)			gtm_deltva(shm_range, NULL, PSL$C_USER)
#define		delete_shm(name_dsc)			del_sec(SEC$M_SYSGBL, name_dsc, NULL)

boolean_t	create_and_map_shm(boolean_t src_or_rcv, struct dsc$descriptor_s *name_dsc, int4 buffsize, sm_uc_ptr_t *shm_range);
int4		map_shm(boolean_t src_or_rcv, struct dsc$descriptor_s *name_dsc, sm_uc_ptr_t *shm_range);
boolean_t	shm_exists(boolean_t src_or_rcv, struct dsc$descriptor_s *name_dsc);
int4		register_with_gsec(struct dsc$descriptor_s *name_dsc, int4 *lockid);
int4		lastuser_of_gsec(int4 gsec_lockid);
uint4		signoff_from_gsec_dbg(unsigned int gsec_lockid);

#define		DETACH_FROM_JNLPOOL(pool_init, jnlpool, jnlpool_ctl)	\
{									\
	if (pool_init)							\
	{								\
		rel_lock(jnlpool.jnlpool_dummy_reg);			\
		detach_shm(jnlpool.shm_range);				\
		signoff_from_gsec(jnlpool.shm_lockid);			\
		memset(&jnlpool, 0, SIZEOF(jnlpool));			\
		jnlpool_ctl = NULL;					\
		pool_init = FALSE;					\
	}								\
}

#endif /* _REPL_SHM_H */
