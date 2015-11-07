/****************************************************************
 *								*
 *	Copyright 2012, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MU_TRUNCATE_DEFINED
#define MU_TRUNCATE_DEFINED

#define BML_BLKS_PER_UCHAR		(BITS_PER_UCHAR / BML_BITS_PER_BLK)

#define GET_STATUS(bp, blknum, bml_status)	\
	bml_status = (bp >> (blknum * BML_BITS_PER_BLK)) & ((1 << BML_BITS_PER_BLK) - 1);

#define CHECK_DBSYNC(reg, save_errno)											\
{															\
	if (0 != save_errno)												\
	{														\
		send_msg_csa(CSA_ARG(REG2CSA(reg)) VARLSTCNT(5) ERR_DBFSYNCERR, 2, DB_LEN_STR(reg), save_errno);	\
		rts_error_csa(CSA_ARG(REG2CSA(reg)) VARLSTCNT(5) ERR_DBFSYNCERR, 2, DB_LEN_STR(reg), save_errno);	\
		assert(FALSE);	/* should not come here as the rts_error above should not return */			\
		rel_crit(reg);												\
		return FALSE;												\
	}														\
}

#define KILL_TRUNC_TEST(label)												\
{															\
	GTM_WHITE_BOX_TEST(label, sigkill, 1);										\
	if (sigkill == 1)												\
		kill( getpid(), 9);											\
}

typedef struct trunc_reg_struct
{
	gd_region		*reg;
	struct trunc_reg_struct	*next;
} trunc_region;

boolean_t mu_truncate(int4 truncate_percent);
STATICFNDCL int4 bml_find_busy_recycled(int4 hint, uchar_ptr_t base_addr, int4 blks_in_lmap, int *bml_status_ptr);

#endif
