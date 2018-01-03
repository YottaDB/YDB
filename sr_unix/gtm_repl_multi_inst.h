/****************************************************************
 *								*
 * Copyright (c) 2017 Fidelity National Information		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef GTM_REPL_MULTI_INST_H
#define GTM_REPL_MULTI_INST_H

error_def(ERR_REPLMULTINSTUPDATE);

/* Issue REPLMULTINSTUPDATE when in TP and touching more than one region */
#define DISALLOW_MULTIINST_UPDATE_IN_TP(TLEVEL, JPLHEAD, CSA, SGMFIRST, DOJNLPOOLINIT)						\
MBSTART {															\
	sgm_info		*si;												\
	jnlpool_addrs_ptr_t	local_jnlpool;											\
																\
	if (IS_MUMPS_IMAGE && TLEVEL && JPLHEAD && JPLHEAD->next)								\
	{	/* in transaction and multiple replication instances */								\
		if (DOJNLPOOLINIT) /* trigger updates may not have initialized the journalpool yet */				\
			JNLPOOL_INIT_IF_NEEDED(CSA, CSA->hdr, CSA->nl, SCNDDBNOUPD_CHECK_TRUE);	/* may set CSA->jnlpool */	\
		for (si = SGMFIRST; si && CSA->jnlpool; si = si->next_sgm_info)							\
		{ /* CSA is connected to a jnlpool, check all regions in the transaction with updates use same jnlpool */	\
			if (si->update_trans && si->tp_csa->jnlpool && (si->tp_csa->jnlpool != CSA->jnlpool))			\
			{													\
				local_jnlpool = si->tp_csa->jnlpool;								\
				assert(local_jnlpool);										\
				assert(CSA->jnlpool == jnlpool);								\
				assert(jnlpool);										\
				assert(WBTEST_ENABLED(WBTEST_NO_REPLINSTMULTI_FAIL));						\
				rts_error_csa(CSA_ARG(CSA) VARLSTCNT(8) ERR_REPLMULTINSTUPDATE, 6,				\
					DB_LEN_STR(local_jnlpool->jnlpool_dummy_reg), DB_LEN_STR(gv_cur_region),		\
					DB_LEN_STR(jnlpool->jnlpool_dummy_reg));						\
			}													\
		}														\
	}															\
} MBEND;
#endif
