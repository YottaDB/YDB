/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
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

#include "gdsroot.h"

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
GBLREF	int		sizeof_is_final_retry_code_num;
GBLREF	int		sizeof_is_final_retry_code_uchar;
GBLREF	int		sizeof_is_final_retry_code_lchar;

static inline boolean_t	is_final_retry_code(enum cdb_sc status)
{
	int idx;
	boolean_t val;

	assert(status <= 'z');

	if (status < 'A')
	{	/* numeric */
		idx = status;
		assert(0 <= idx);
		assert((sizeof_is_final_retry_code_num / SIZEOF(boolean_t)) > idx);
		val = is_final_retry_code_num[idx];
	}
	else if (status <= 'Z')
	{	/* upper case */
		idx = status - 'A';
		assert(0 <= idx);
		assert((sizeof_is_final_retry_code_uchar / SIZEOF(boolean_t)) > idx);
		val = is_final_retry_code_uchar[idx];
	} else
	{	/* lower case */
		idx = status - 'a';
		assert(0 <= idx);
		assert((sizeof_is_final_retry_code_lchar / SIZEOF(boolean_t)) > idx);
		val = is_final_retry_code_lchar[idx];
	}
	return val;
}

/* This macro is used in places that don't rely on t_retry() to handle the possibility of a retry in the final try.
 * For example, database trigger handling code assumes that a structural issue with a trigger global is due to a
 * concurrent update and not a broken entry in the DB. Once the final retry has been exhausted, the trigger code
 * path issues a TRIGDEFBAD error. is_final_retry_code enhances that check against CDB_STAGNATE to ensure that the
 * final retry has truly been exhausted.
 */
#define UPDATE_CAN_RETRY(TRIES, CURRSTATUS)							\
	((CDB_STAGNATE > TRIES) || (is_final_retry_code(CURRSTATUS)))

#define TP_TRACE_HIST_MOD(BLK_NUM, BLK_TARGET, N, CSD, HISTTN, BTTN, LEVEL)							\
MBSTART {															\
	DEBUG_ONLY(GBLREF	uint4	dollar_tlevel;)										\
	DCL_THREADGBL_ACCESS;													\
																\
	SETUP_THREADGBL_ACCESS;													\
	if (TREF(tprestart_syslog_delta))											\
	{	/* next 4 lines of code are identical to TP_TRACE_HIST (below), but repetion saves an if when it matters */	\
		assert(dollar_tlevel);												\
		TAREF1(t_fail_hist_blk, t_tries) = ((block_id)BLK_NUM);								\
		TAREF1(tp_fail_hist, t_tries) = (gv_namehead *)(((block_id)BLK_NUM & ~(-BLKS_PER_LMAP)) ? BLK_TARGET : NULL);	\
		TAREF1(tp_fail_hist_reg, t_tries) = gv_cur_region;								\
		(CSD)->tp_cdb_sc_blkmod[(N)]++;											\
		TREF(blkmod_fail_level) = (LEVEL);										\
		TREF(blkmod_fail_type) = (N);											\
		TAREF1(tp_fail_bttn, t_tries) = (BTTN);										\
		TAREF1(tp_fail_histtn, t_tries) = (HISTTN);									\
	}															\
} MBEND

#define NONTP_TRACE_HIST_MOD(BLK_SRCH_STAT, N)				\
MBSTART {								\
	DEBUG_ONLY(GBLREF	uint4	dollar_tlevel;)			\
	DCL_THREADGBL_ACCESS;						\
									\
	SETUP_THREADGBL_ACCESS;						\
	assert(!dollar_tlevel);						\
	TREF(blkmod_fail_type) = (N);					\
	TREF(blkmod_fail_level) = (BLK_SRCH_STAT)->level;		\
	TAREF1(t_fail_hist_blk, t_tries) = (BLK_SRCH_STAT)->blk_num;	\
} MBEND

#define TP_TRACE_HIST(BLK_NUM, BLK_TARGET) 											\
MBSTART {															\
	DEBUG_ONLY(GBLREF	uint4	dollar_tlevel;)										\
	DCL_THREADGBL_ACCESS;													\
																\
	SETUP_THREADGBL_ACCESS;													\
	if (TREF(tprestart_syslog_delta))											\
	{															\
		assert(dollar_tlevel);												\
		TAREF1(t_fail_hist_blk, t_tries) = ((block_id)BLK_NUM);								\
		TAREF1(tp_fail_hist, t_tries) = (gv_namehead *)(((block_id)BLK_NUM & ~(-BLKS_PER_LMAP)) ? BLK_TARGET : NULL);	\
		TAREF1(tp_fail_hist_reg, t_tries) = gv_cur_region;								\
	}															\
} MBEND

#endif
