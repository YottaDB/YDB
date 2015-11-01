/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

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
 * BLK_INIT, BLK_FINI space ---> CDB_CW_SET_SIZE * (BLK_SEG_ARRAY_SIZE * sizeof(blk_segment))
 * BLK_ADDR leaf-level space---> 2 * cs_data->blk_size         (for current and new sibling)
 * BLK_ADDR index-level space--> (MAX_BT_DEPTH - 1) * (2 * (MAX_KEY_SZ + sizeof(rec_hdr) + sizeof(block_id)) + sizeof(rec_hdr))
 * 2 extra space            ---> cs_data->blk_size + BSTAR_REC_SIZE		(needed in case of global-variable creation)
 * Bitmap BLK_ADDR space    ---> (MAX_BT_DEPTH + 1) * (sizeof(block_id) * (BLKS_PER_LMAP + 1))
 *
 */

#define	UPDATE_ELEMENT_ALIGN_SIZE	8
#define	UPDATE_ARRAY_ALIGN_SIZE		(1 << 14)		/* round up the update array to a 16K boundary */
#define	MAX_BITMAP_UPDATE_ARRAY_SIZE	((MAX_BT_DEPTH + 1) * ROUND_UP2(sizeof(block_id) * (BLKS_PER_LMAP + 1), 		\
												UPDATE_ELEMENT_ALIGN_SIZE))
#define MAX_NON_BITMAP_UPDATE_ARRAY_SIZE(csd)											\
		(CDB_CW_SET_SIZE * ROUND_UP2(BLK_SEG_ARRAY_SIZE * sizeof(blk_segment), UPDATE_ELEMENT_ALIGN_SIZE)		\
		 + 2 * ROUND_UP2(csd->blk_size, UPDATE_ELEMENT_ALIGN_SIZE)							\
		 + (MAX_BT_DEPTH - 1) * 											\
			(3 * ROUND_UP2(sizeof(rec_hdr), UPDATE_ELEMENT_ALIGN_SIZE) 						\
				+ 2 * ROUND_UP2(sizeof(block_id), UPDATE_ELEMENT_ALIGN_SIZE) + 2 * ROUND_UP2(MAX_KEY_SZ, 8))	\
		 + ROUND_UP2(csd->blk_size, UPDATE_ELEMENT_ALIGN_SIZE) + ROUND_UP2(BSTAR_REC_SIZE, UPDATE_ELEMENT_ALIGN_SIZE))

#define UA_SIZE(X)		(X->max_update_array_size)
#define UA_NON_BM_SIZE(X)	(X->max_non_bm_update_array_size)

#define	ENSURE_UPDATE_ARRAY_SPACE(space_needed)											\
{																\
	if (ROUND_DOWN2(update_array + update_array_size - update_array_ptr, UPDATE_ELEMENT_ALIGN_SIZE) < (space_needed))	\
	{	/* the space remaining is too small for safety - chain on a new array */					\
		curr_ua = curr_ua->next_ua											\
			= (ua_list *)malloc(sizeof(ua_list));									\
		curr_ua->next_ua = NULL;											\
		curr_ua->update_array_size = update_array_size = MIN(MAX(cumul_update_array_size, (space_needed)), BIG_UA);	\
		cumul_update_array_size += update_array_size;									\
		curr_ua->update_array = update_array 										\
					= update_array_ptr									\
					= (char *)malloc(update_array_size);							\
	}															\
}

/* ***************************************************************************
 *	BLK_INIT(BNUM, ARRAY) allocates:
 *		blk_segment ARRAY[BLK_SEG_ARRAY_SIZE]
 *	at the next octaword-aligned location in the update array and sets
 *		BNUM = &ARRAY[1]
 */

#define BLK_INIT(BNUM, ARRAY) 									\
{												\
	update_array_ptr = (char*)ROUND_UP2((int)update_array_ptr, UPDATE_ELEMENT_ALIGN_SIZE);	\
	(ARRAY) = (blk_segment*)update_array_ptr; 						\
	update_array_ptr += BLK_SEG_ARRAY_SIZE*sizeof(blk_segment); 				\
	assert((update_array + update_array_size) - update_array_ptr >= 0); 			\
	(BNUM) = (ARRAY + 1); 									\
	blk_seg_cnt = sizeof(blk_hdr);								\
}

/* ***************************************************************************
 *	BLK_SEG(BNUM, ADDR, LEN) adds a new entry to the blk_segment array
 */

#define BLK_SEG(BNUM, ADDR, LEN) 					\
{									\
	(BNUM)->addr = (ADDR); 						\
	(BNUM)->len  = (LEN); 						\
	blk_seg_cnt += (unsigned short) (LEN); 				\
	assert((char *)BNUM - update_array_ptr < 0); 			\
	(BNUM)++;							\
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
	(blk_seg_cnt <= blk_size  &&  blk_seg_cnt >= sizeof(blk_hdr))			\
		? (ARRAY)[0].addr = (uchar_ptr_t)(BNUM), (ARRAY)[0].len = blk_seg_cnt	\
		: 0									\
)

/* ***************************************************************************
 *	BLK_ADDR(X,Y,Z) allocates a space of length Y in the update array
 *	and sets pointer X (of type Z) to the beginning of that space
 */

#ifdef DEBUG
#define BLK_ADDR(X,Y,Z) 								\
(											\
	update_array_ptr = (char*)(((int)update_array_ptr + 7) & ~7), 			\
	assert((update_array + update_array_size - Y) - update_array_ptr >= 0), 	\
	(X) = (Z*)update_array_ptr, update_array_ptr += Y				\
)
#else
#define BLK_ADDR(X,Y,Z)									\
(											\
	update_array_ptr = (char*)(((int)update_array_ptr + 7) & ~7), 			\
	(X) = (Z*)update_array_ptr, update_array_ptr += Y				\
)
#endif
