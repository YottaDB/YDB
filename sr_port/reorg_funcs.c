/****************************************************************
 *								*
 *	Copyright 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "cdb_sc.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "gdsblkops.h"
#include "gdskill.h"
#include "gdscc.h"
#include "copy.h"
#include "mu_reorg.h"
#include "util.h"
#include "min_max.h"

GBLREF 	sgmnt_data_ptr_t 	cs_data;
GBLREF	unsigned int		t_tries;

#ifdef DEBUG
# define DBG_VERIFY_ACCESS(PTR)				\
{	/* Ensure accessible pointer (no SIG-11) */	\
	unsigned char	c;				\
							\
	c = *(unsigned char *)(PTR);			\
}
#else
# define DBG_VERIFY_ACCESS(PTR)
#endif

/*
 * 	Get length of the global variable name contained in the key starting at KEY_BASE.
 *	NOTE: If keys reside outside a GDS block (e.g. in cs_data), allocated buffer should have capacity at least MAX_KEY_SZ.
 * 	Input:
 * 		BLK_BASE := 	if non-NULL, base of current block
 * 				if NULL, it means we're dealing with a key that is not within a database block
 *		KEY_BASE :=	starting address of key
 * 	Output:
 * 		GBLNAME_LEN := 	length of global variable name
 */
int get_gblname_len(sm_uc_ptr_t blk_base, sm_uc_ptr_t key_base)
{
	sm_uc_ptr_t		rPtr1, blk_end;

	blk_end = (NULL != blk_base) ? (blk_base + cs_data->blk_size) : (key_base + MAX_KEY_SZ);
	DBG_VERIFY_ACCESS(blk_end - 1);
	for (rPtr1 = key_base; ; )
	{
		if (blk_end <= rPtr1)
			break;
		if (KEY_DELIMITER == *rPtr1++)
			break;
	}
	return (int)(rPtr1 - key_base);
}

/*
 * 	Get length of the key starting at KEY_BASE.
 *	NOTE: If keys reside outside a GDS block (e.g. in cs_data), allocated buffer should have capacity at least MAX_KEY_SZ.
 * 	Currently, all get_key_len() calls take a key in shared memory; the only "allocated buffer" is csd->reorg_restart_key.
 *	Input:
 * 		BLK_BASE := 	if non-NULL, base of current block
 * 				if NULL, it means we're dealing with a key that is not within a database block
 *		KEY_BASE :=	starting address of key
 * 	Output:
 * 		KEY_LEN	:= 	length of key, including 2 null bytes at the end
 */
int get_key_len(sm_uc_ptr_t blk_base, sm_uc_ptr_t key_base)
{
	sm_uc_ptr_t		rPtr1, blk_end;

	blk_end = (NULL != blk_base) ? (blk_base + cs_data->blk_size) : (key_base + MAX_KEY_SZ);
	DBG_VERIFY_ACCESS(blk_end - 1);
	for (rPtr1 = key_base; ; )
	{
		if (blk_end <= rPtr1 + 1)
			break;
		if ((KEY_DELIMITER == *rPtr1++) && (KEY_DELIMITER == *rPtr1))
			break;
	}
	return (int)(rPtr1 + 1 - key_base);
}

/*
 *	Get compression count of SECOND_KEY with respect to FIRST_KEY.
 * 	NOTE: Each key should reside in a private buffer with capacity at least MAX_KEY_SZ.
 */
int get_cmpc(sm_uc_ptr_t first_key, sm_uc_ptr_t second_key)
{
	sm_uc_ptr_t		rPtr1, rPtr2;
	int			cmpc;

	DBG_VERIFY_ACCESS(first_key + MAX_KEY_SZ - 1);
	DBG_VERIFY_ACCESS(second_key + MAX_KEY_SZ - 1);
	/* We don't expect the inputs to be equal, hence the assert. It shouldn't matter, though. If the keys' contents are equal,
	 * we return an indeterminate value between the key length and MAX_KEY_SZ. The value depends on the garbage bytes past the
	 * terminating null bytes. But we don't care because we don't compress a key off an identical key in the final retry.
	 */
	assert(first_key != second_key);
	for (rPtr1 = first_key, rPtr2 = second_key, cmpc = 0; cmpc < MAX_KEY_SZ; cmpc++)
	{
		if (*rPtr1++ != *rPtr2++)
			break;
	}
	return cmpc;
}

/*
 * 	Copy record info (record size and key) out of a block.
 *	Input:
 *		LEVEL :=	level of current block
 *		BLK_BASE := 	base of current block
 *		REC_BASE :=	starting address of record in current block
 * 		KEY := 		previous key; first KEY_CMPC bytes are retained in output KEY
 *	Output:
 *		STATUS := 	status of read
 *		REC_SIZE :=	record size
 *		KEY_CMPC := 	key compression count
 *		KEY_LEN := 	key length
 *		KEY :=		local copy of key copied out of record
 */
enum cdb_sc read_record(int *rec_size_ptr, int *key_cmpc_ptr, int *key_len_ptr, sm_uc_ptr_t key,
		int level, sm_uc_ptr_t blk_base, sm_uc_ptr_t rec_base)
{
	sm_uc_ptr_t	rPtr1, rPtr2, blk_end, rPtr1_end, rPtr2_end;
	unsigned short	temp_ushort;
	int		key_cmpc, rec_size, key_len;
	boolean_t	invalid;

	blk_end = blk_base + cs_data->blk_size;
	DBG_VERIFY_ACCESS(blk_end - 1);
	if (blk_end <= (rec_base + SIZEOF(rec_hdr)))
	{
		assert(CDB_STAGNATE > t_tries);
		return cdb_sc_blkmod;
	}
	GET_USHORT(temp_ushort, &(((rec_hdr_ptr_t)rec_base)->rsiz));
	rec_size = temp_ushort;
	key_cmpc = EVAL_CMPC((rec_hdr_ptr_t)rec_base);
	if ((0 != level) && (BSTAR_REC_SIZE == rec_size))
	{
		key_len = 0;
		*key_cmpc_ptr = key_cmpc;
		*rec_size_ptr = rec_size;
		*key_len_ptr = key_len;
		return cdb_sc_starrecord;
	}
	rPtr1_end = key + MAX_KEY_SZ - 1;
	DBG_VERIFY_ACCESS(rPtr1_end);
	rPtr2_end = MIN(blk_end - 1, rec_base + SIZEOF(rec_hdr) + MAX_KEY_SZ - 1);
	for (rPtr1 = key + key_cmpc, rPtr2 = rec_base + SIZEOF(rec_hdr); (rPtr1 < rPtr1_end) && (rPtr2 < rPtr2_end); )
	{
		if ((KEY_DELIMITER == (*rPtr1++ = *rPtr2++)) && (KEY_DELIMITER == *rPtr2))	/* note assignment */
			break;
	}
	*rPtr1++ = *rPtr2++;
	key_len = (int)(rPtr2 - rec_base - SIZEOF(rec_hdr));
	invalid = INVALID_RECORD(level, rec_size, key_len, key_cmpc);
	if (invalid || ((KEY_DELIMITER != *(rPtr1 - 1)) || (KEY_DELIMITER != *(rPtr1 - 2))))
	{
		assert(CDB_STAGNATE > t_tries);
		return cdb_sc_blkmod;
	}
	*key_cmpc_ptr = key_cmpc;
	*rec_size_ptr = rec_size;
	*key_len_ptr = key_len;
	return cdb_sc_normal;
}
