/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __GDSBLK_OPS_H__
#define __GDSBLK_OPS_H__

/* Block segment array holds the list of memcpys needed to build the updated block.  The first entry in the array holds the
	length of the updated block and the address of the last entry in the array that holds valid data.
	The arrays are processed at t_end time, and the copies are done starting with the last element in the array
*/

/* HEADER-FILE-DEPENDENCIES : min_max.h */

typedef struct
{
	sm_uc_ptr_t	addr;
	sm_ulong_t	len;
} blk_segment;

#define BLK_SEG_ARRAY_SIZE	20

/* although kills may have MAX_BT_DEPTH * 2 - 1 elements, each element is limited to key_size and gives a smaller max than puts */

/* ***************************************************************************
 * The following is the splitup of the calculation of maximum-update-array-size for one non-TP action (for a PUT)
 *
 * BLK_INIT, BLK_FINI space ---> CDB_CW_SET_SIZE * (BLK_SEG_ARRAY_SIZE * SIZEOF(blk_segment))
 * BLK_ADDR leaf-level space---> 2 * cs_data->blk_size         (for current and new sibling)
 * BLK_ADDR index-level space--> (MAX_BT_DEPTH - 1) * (2 * (MAX_KEY_SZ + SIZEOF(rec_hdr) + SIZEOF(block_id)) + SIZEOF(rec_hdr))
 * 2 extra space            ---> cs_data->blk_size + BSTAR_REC_SIZE		(needed in case of global-variable creation)
 * Bitmap BLK_ADDR space    ---> (MAX_BT_DEPTH + 1) * (SIZEOF(block_id) * (BLKS_PER_LMAP + 1))
 *
 */

#define	UPDATE_ELEMENT_ALIGN_SIZE	8
#define	UPDATE_ARRAY_ALIGN_SIZE		(1 << 14)		/* round up the update array to a 16K boundary */
#define	MAX_BITMAP_UPDATE_ARRAY_SIZE	((MAX_BT_DEPTH + 1) * ROUND_UP2(SIZEOF(block_id) * (BLKS_PER_LMAP + 1), 		\
												UPDATE_ELEMENT_ALIGN_SIZE))
#define MAX_NON_BITMAP_UPDATE_ARRAY_SIZE(csd)											\
		(CDB_CW_SET_SIZE * ROUND_UP2(BLK_SEG_ARRAY_SIZE * SIZEOF(blk_segment), UPDATE_ELEMENT_ALIGN_SIZE)		\
		 + 2 * ROUND_UP2(csd->blk_size, UPDATE_ELEMENT_ALIGN_SIZE)							\
		 + (MAX_BT_DEPTH - 1) * 											\
			(3 * ROUND_UP2(SIZEOF(rec_hdr), UPDATE_ELEMENT_ALIGN_SIZE) 						\
				+ 2 * ROUND_UP2(SIZEOF(block_id), UPDATE_ELEMENT_ALIGN_SIZE) + 2 * ROUND_UP2(MAX_KEY_SZ, 8))	\
		 + ROUND_UP2(csd->blk_size, UPDATE_ELEMENT_ALIGN_SIZE) + ROUND_UP2(BSTAR_REC_SIZE, UPDATE_ELEMENT_ALIGN_SIZE))

#define UA_SIZE(X)		(uint4)(X->max_update_array_size)
#define UA_NON_BM_SIZE(X)	(uint4)(X->max_non_bm_update_array_size)

#define	ENSURE_UPDATE_ARRAY_SPACE(space_needed)											\
{																\
	GBLREF	ua_list		*first_ua, *curr_ua;										\
	GBLREF	char		*update_array, *update_array_ptr;								\
	GBLREF	uint4		update_array_size, cumul_update_array_size;							\
	GBLREF	uint4		dollar_tlevel;											\
	ua_list			*tmpua;												\
																\
	assert((0 != update_array_size) && (NULL != update_array));								\
	if (ROUND_DOWN2(update_array + update_array_size - update_array_ptr, UPDATE_ELEMENT_ALIGN_SIZE) < (space_needed))	\
	{	/* the space remaining is too small for safety - chain on a new array */					\
		assert((NULL != first_ua) && (NULL != curr_ua)); 								\
		if ((NULL == curr_ua) || (NULL == curr_ua->next_ua))								\
		{														\
			/* care should be taken to ensure things will work right even if malloc() errors out with ERR_MEMORY.	\
			 * that is why multiple assignments are not done in a single line that has a malloc() call in it.	\
			 * in addition, the field taking in the result of malloc() should be initialized to NULL before the 	\
			 * call. This is because tp_clean_up() (invoked in case of error handling) relies on the integrity of 	\
			 * this update array linked list in order to do its cleanup. Not following the above rules will cause 	\
			 * difficult-to-debug memory related problems (even corruption) */					\
			tmpua = (ua_list *)malloc(SIZEOF(ua_list));								\
			memset(tmpua, 0, SIZEOF(ua_list));	/* initialize tmpua->update_array and tmpua->next_ua to NULL */	\
			/* it is important that all parameters in the MIN-MAX calculation below be unsigned numbers */		\
			tmpua->update_array_size = MIN(MAX(cumul_update_array_size, (space_needed)), BIG_UA);			\
			tmpua->update_array = (char *)malloc(tmpua->update_array_size);						\
			/* update globals only after above mallocs succeed */							\
			cumul_update_array_size += tmpua->update_array_size;							\
			if (NULL == curr_ua) /* in PRO, don't take chances, reset first_ua/curr_ua to newly created upd array */\
			{													\
				if (dollar_tlevel && (NULL != first_ua)) /* if already in TP, we will lose all updates until now\
									    if we reset first_ua. do not proceed in this case */\
					GTMASSERT;										\
				first_ua = curr_ua = tmpua;									\
			} else													\
				curr_ua = curr_ua->next_ua = tmpua;								\
		} else														\
		{	/* No need to do malloc as curr_ua->next_ua could be REUSED */						\
			curr_ua = curr_ua->next_ua;										\
			/* No need to reset cumul_update_array_size as no ua_list is deleted/added from/to the first_ua		\
			 * linked list */											\
		}														\
		assert((NULL != first_ua) && (NULL != curr_ua));								\
		update_array_size = curr_ua->update_array_size;									\
		update_array = update_array_ptr = curr_ua->update_array;							\
	}															\
}

/* The following macro resets update_array_ptr to point to update_array.
 * This needs to be done before using any BLK_* macros as part of a non-TP transaction.
 */
#define	RESET_UPDATE_ARRAY					\
{								\
	GBLREF	char	*update_array, *update_array_ptr;	\
								\
	assert(NULL != update_array);				\
	/* reset update_array to the start */			\
	update_array_ptr = update_array;			\
}

/* the following macro does what RESET_UPDATE_ARRAY does and additionally does some integrity checks */
#define	CHECK_AND_RESET_UPDATE_ARRAY								\
{												\
	GBLREF	uint4			dollar_tlevel;						\
	GBLREF	unsigned char		cw_set_depth;						\
												\
	assert(!dollar_tlevel);	/* TP should never use this update_array */			\
	assert(0 == cw_set_depth);	/* ensure we never reset an active update_array */	\
	RESET_UPDATE_ARRAY;									\
}

/* ***************************************************************************
 *	BLK_INIT(BNUM, ARRAY) allocates:
 *		blk_segment ARRAY[BLK_SEG_ARRAY_SIZE]
 *	at the next octaword-aligned location in the update array and sets
 *		BNUM = &ARRAY[1]
 */

#define BLK_INIT(BNUM, ARRAY)										\
{													\
	GBLREF	char	*update_array, *update_array_ptr;						\
													\
	update_array_ptr = (char*)ROUND_UP2((INTPTR_T)update_array_ptr, UPDATE_ELEMENT_ALIGN_SIZE);	\
	(ARRAY) = (blk_segment*)update_array_ptr;							\
	update_array_ptr += (BLK_SEG_ARRAY_SIZE * SIZEOF(blk_segment));					\
	assert((update_array + update_array_size) - update_array_ptr >= 0);				\
	(BNUM) = (ARRAY + 1);										\
	blk_seg_cnt = SIZEOF(blk_hdr);									\
}

/* ***************************************************************************
 *	BLK_SEG(BNUM, ADDR, LEN) adds a new entry to the blk_segment array
 */

#define BLK_SEG(BNUM, ADDR, LEN)										\
{														\
	sm_ulong_t		lcl_len1;									\
	GBLREF uint4		dollar_tlevel;									\
														\
	/* Note that name of len variable "lcl_len1" should be different					\
	 * from the name used in the REORG_BLK_SEG macro as otherwise						\
	 * the below assignment will leave lcl_len1 uninitialized.						\
	 */													\
	lcl_len1 = (LEN);											\
	/* The function "gvcst_blk_build" (which uses this update array to build the block) relies on "len" to	\
	 * be positive. This macro is called from both non-TP and TP code. In non-TP, we know of a few callers	\
	 * (dse etc.) that could pass in a negative length to the BLK_SEG macro. Those are okay since we are	\
	 * guaranteed the validation (in t_end inside of crit) would catch this case and force a restart thus	\
	 * avoiding a call to gvcst_blk_build. But in TP, validations happen before commit time in tp_hist and	\
	 * it has a few optimizations where for globals with NOISOLATION turned ON, it allows validation to	\
	 * succeed even if the block contents changed concurrently. This means update arrays with negative	\
	 * lengths could find their way to gvcst_blk_build even after tp_hist. Since those lengths are passed	\
	 * directly to memmove which treats it as an unsigned quantity, it means huge memmoves that are likely	\
	 * to cause memory corruption and/or SIG-11. Therefore it is absolutely necessary that if we are in TP	\
	 * the caller does not pass in a negative length. Assert that.						\
	 */													\
	assert(!dollar_tlevel || (0 <= (sm_long_t)lcl_len1));							\
	(BNUM)->addr = (ADDR);											\
	(BNUM)->len  = lcl_len1;										\
	blk_seg_cnt += (int)lcl_len1;										\
	assert((char *)BNUM - update_array_ptr < 0);								\
	(BNUM)++;												\
}

/* ***************************************************************************
 *	BLK_FINI(BNUM,ARRAY) finishes the update array by
 *		BNUM->addr = 0
 *		BNUM--
 *	if the blk_seg_cnt is within range, then
 *		ARRAY[0].addr = BNUM		(address of last entry containing data)
 *		ARRAY[0].len  = blk_seg_cnt	(total size of all block segments)
 *		and it returns the value of blk_seg_cnt,
 *	otherwise, it returns zero and the caller should invoke t_retry
 */

#define BLK_FINI(BNUM,ARRAY) 								\
(											\
	(BNUM--)->addr = (uchar_ptr_t)0,						\
	(blk_seg_cnt <= blk_size  &&  blk_seg_cnt >= SIZEOF(blk_hdr))			\
		? (ARRAY)[0].addr = (uchar_ptr_t)(BNUM), (ARRAY)[0].len = blk_seg_cnt	\
		: 0									\
)

/* ***************************************************************************
 *	BLK_ADDR(X,Y,Z) allocates a space of length Y in the update array
 *	and sets pointer X (of type Z) to the beginning of that space
 */

#ifdef DEBUG
#define BLK_ADDR(X,Y,Z)									\
(											\
	update_array_ptr = (char*)(((INTPTR_T)update_array_ptr + 7) & ~7),		\
	assert((update_array + update_array_size - Y) - update_array_ptr >= 0),		\
	(X) = (Z*)update_array_ptr, update_array_ptr += (INTPTR_T)Y			\
)
#else
#define BLK_ADDR(X,Y,Z)									\
(											\
	update_array_ptr = (char*)(((INTPTR_T)update_array_ptr + 7) & ~7),		\
	(X) = (Z*)update_array_ptr, update_array_ptr += (INTPTR_T)Y			\
)
#endif

/* ********************************************************************************
 *	REORG_BLK_SEG(BNUM, ADDR, LEN) is the same as the BLK_SEG macro
 *	except that it takes a private copy of the input memory and adds that
 *	to the update array. This is necessary in case of MUPIP REORG for two
 *	operations (Coalesce and Swap). In either cases, the contents of one block
 *	will rely on the contents of itself and another block. This is not currently
 *	supported by t_end where the assumption is that all contents needed to build
 *	a buffer are available in that buffer itself (this greatly simplifies the
 *	process of pinning of buffers in shared memory). To avoid cross-links to other
 *	buffers, we need to take a copy of the other buffer's contents from shared
 *	memory into private memory before adding it to the update array. This macro
 *	should be called only by MUPIP REORG. An assert has been added to that effect.
 */
#define REORG_BLK_SEG(BNUM, ADDR, LEN)						\
{										\
	char			*lcl_ptr;					\
	sm_ulong_t		lcl_len;					\
										\
	GBLREF	boolean_t	mu_reorg_process;				\
										\
	assert(mu_reorg_process);						\
	lcl_len = (LEN);							\
	if ((0 > (sm_long_t)lcl_len) || ((blk_seg_cnt + lcl_len) > blk_size))	\
	{									\
		assert(CDB_STAGNATE > t_tries);					\
		return cdb_sc_blkmod;						\
	}									\
	BLK_ADDR(lcl_ptr, lcl_len, char);					\
	memcpy(lcl_ptr, (ADDR), lcl_len);					\
	BLK_SEG((BNUM), (sm_uc_ptr_t)lcl_ptr, lcl_len);				\
}

#endif
