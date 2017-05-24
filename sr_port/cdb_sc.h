/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef CDB_SC
#define CDB_SC

/*********************************  WARNING:  ***********************************
*   Several of these codes are concurrently defined in GVCST_BLK_SEARCH.MAR,	*
*   GVCST_SEARCH.MAR, MUTEX.MAR, and MUTEX_STOPREL.MAR.  If their positions	*
*   are changed here, their definitions must be modified there as well!		*
********************************************************************************/

enum cdb_sc
{
#define CDB_SC_NUM_ENTRY(code, final_retry_ok, value)			code = value,
#define CDB_SC_UCHAR_ENTRY(code, final_retry_ok, is_wcs_code, value)	code = value,
#define CDB_SC_LCHAR_ENTRY(code, final_retry_ok, is_wcs_code, value)	code = value,
#include "cdb_sc_table.h"
#undef CDB_SC_NUM_ENTRY
#undef CDB_SC_UCHAR_ENTRY
#undef CDB_SC_LCHAR_ENTRY
};

GBLREF	boolean_t	is_final_retry_code_num[];
GBLREF	boolean_t	is_final_retry_code_uchar[];
GBLREF	boolean_t	is_final_retry_code_lchar[];

#define	IS_FINAL_RETRY_CODE(STATUS)								\
	(DBG_ASSERT(STATUS <= 'z')								\
	((STATUS < 'A')										\
		? is_final_retry_code_num[STATUS]			/* numeric */		\
		: ((STATUS <= 'Z')								\
			? is_final_retry_code_uchar[STATUS - 'A']	/* upper case */	\
			: is_final_retry_code_lchar[STATUS - 'a'])))	/* lower case */

/* This macro is used in places that don't rely on t_retry() to handle the possibility of a retry in the final try.
 * For example, database trigger handling code assumes that a structural issue with a trigger global is due to a
 * concurrent update and not a broken entry in the DB. Once the final retry has been exhausted, the trigger code
 * path issues a TRIGDEFBAD error. IS_FINAL_RETRY_CODE enhances that check against CDB_STAGNATE to ensure that the
 * final retry has truly been exhausted.
 */
#define UPDATE_CAN_RETRY(TRIES, CURRSTATUS)							\
	((CDB_STAGNATE > TRIES) || (IS_FINAL_RETRY_CODE(CURRSTATUS)))

#define TP_TRACE_HIST_MOD(BLK_NUM, BLK_TARGET, N, CSD, HISTTN, BTTN, LEVEL)						\
{															\
	GBLREF	block_id		t_fail_hist_blk[];								\
	GBLREF	gd_region		*tp_fail_hist_reg[];								\
	GBLREF	gv_namehead		*tp_fail_hist[];								\
	GBLREF	int4			blkmod_fail_type;								\
	GBLREF	int4			blkmod_fail_level;								\
	GBLREF	trans_num		tp_fail_histtn[], tp_fail_bttn[];						\
	DEBUG_ONLY(GBLREF	uint4	dollar_tlevel;)									\
	DCL_THREADGBL_ACCESS;												\
															\
	SETUP_THREADGBL_ACCESS;												\
	assert(dollar_tlevel);												\
	if (TREF(tprestart_syslog_delta))										\
	{														\
		tp_fail_hist_reg[t_tries] = gv_cur_region;								\
		t_fail_hist_blk[t_tries] = ((block_id)BLK_NUM);								\
		tp_fail_hist[t_tries] = (gv_namehead *)(((int)BLK_NUM & ~(-BLKS_PER_LMAP)) ? BLK_TARGET : NULL);	\
		(CSD)->tp_cdb_sc_blkmod[(N)]++;										\
		blkmod_fail_type = (N);											\
		blkmod_fail_level = (LEVEL);										\
		tp_fail_histtn[t_tries] = (HISTTN);									\
		tp_fail_bttn[t_tries] = (BTTN);										\
	}														\
}

#define NONTP_TRACE_HIST_MOD(BLK_SRCH_STAT, N)			\
{								\
	GBLREF	int4		blkmod_fail_type;		\
	GBLREF	int4		blkmod_fail_level;		\
	GBLREF	block_id	t_fail_hist_blk[];		\
	DEBUG_ONLY(GBLREF	uint4	dollar_tlevel;)		\
								\
	assert(!dollar_tlevel);					\
	t_fail_hist_blk[t_tries] = (BLK_SRCH_STAT)->blk_num;	\
	blkmod_fail_type = (N);					\
	blkmod_fail_level = (BLK_SRCH_STAT)->level;		\
}

#endif
