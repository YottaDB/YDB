/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* mu_reorg.h
	This has definitions for mupip reorg utility.
	To include this gv_cur_region must be defined and
	cdb_sc.h and copy.h must be included.
*/
#ifndef FLUSH
#define FLUSH 1
#endif
#ifndef NOFLUSH
#define NOFLUSH 0
#endif

/* Following definitions give some default values for reorg */
#define DATA_FILL_TOLERANCE 10
#define INDEX_FILL_TOLERANCE 10

/*********************************************************************
	block_number is used for swap
 *********************************************************************/
#define INCR_BLK_NUM(block_number)      		\
	(block_number)++; 				\
        if (IS_BITMAP_BLK(block_number)) 		\
		(block_number)++;			\
	assert(!IS_BITMAP_BLK(block_number));

/* DECR_BLK_NUM has no check for bit-map because it is always followed by INCR_BLK_NUM */
#define DECR_BLK_NUM(block_number)      (block_number)--

/*
 *	If INVALID_RECORD evaluates to TRUE, it means some necessary record/key relations are incongruent, and we cannot proceed
 *	with update array calculations. Restart.
 * 	Input:
 *		LEVEL :=	level of current block
 * 		REC_SIZE :=	record size
 *		KEYLEN := 	key length
 *		KEYCMPC :=	key compression count
 */
#define INVALID_RECORD(LEVEL, REC_SIZE, KEYLEN, KEYCMPC)			\
	(	   (MAX_KEY_SZ < ((int)(KEYLEN) + (KEYCMPC)))			\
		|| (BSTAR_REC_SIZE > ((REC_SIZE) + (0 == (LEVEL) ? 1 : 0)))	\
		|| ((0 == (LEVEL)) && (2 >= (KEYLEN)))				\
	)

/* Key allocation better be big enough. We can check array sizes, so we do. But we can't check arbitrary pointers, so if a pointer
 * is passed to DBG_CHECK_KEY_ALLOCATION_SIZE, we ignore it. The complication below is for distinguishing arrays from pointers
 * on Tru64, where pointers can be either 32-bit or 64-bit.
 */
#if defined(__osf__) && defined(DEBUG)
# pragma pointer_size(save)
# pragma pointer_size(long)
typedef char *dbg_osf_long_char_ptr_t;	/* 64-bit */
# pragma pointer_size(short)
typedef char *dbg_osf_short_char_ptr_t;	/* 32-bit */
# pragma pointer_size(restore)
# define DBG_CHECK_KEY_ALLOCATION_SIZE(KEY)	assert((MAX_KEY_SZ < ARRAYSIZE((KEY)))						\
		|| (SIZEOF(dbg_osf_long_char_ptr_t) == SIZEOF((KEY))) || (SIZEOF(dbg_osf_short_char_ptr_t) == SIZEOF((KEY))))
#else	/* normal platforms; non-Tru64 */
typedef char *dbg_osf_long_char_ptr_t;
typedef char *dbg_osf_short_char_ptr_t;
# define DBG_CHECK_KEY_ALLOCATION_SIZE(KEY)	assert((MAX_KEY_SZ < ARRAYSIZE((KEY))) || (SIZEOF(char_ptr_t) == SIZEOF((KEY))))
#endif

#define GET_CMPC(KEY_CMPC, FIRST_KEY, SECOND_KEY)	\
{							\
	DBG_CHECK_KEY_ALLOCATION_SIZE(FIRST_KEY);	\
	DBG_CHECK_KEY_ALLOCATION_SIZE(SECOND_KEY);	\
	KEY_CMPC = get_cmpc(FIRST_KEY, SECOND_KEY);	\
}

#define READ_RECORD(STATUS, REC_SIZE_PTR, KEY_CMPC_PTR, KEY_LEN_PTR, KEY, LEVEL, BLK_BASE, REC_BASE)	\
{													\
	DBG_CHECK_KEY_ALLOCATION_SIZE(KEY);								\
	STATUS = read_record(REC_SIZE_PTR, KEY_CMPC_PTR, KEY_LEN_PTR, KEY, LEVEL, BLK_BASE, REC_BASE);	\
}

enum reorg_options {	DEFAULT = 0,
			SWAPHIST = 0x0001,
			NOCOALESCE = 0x0002,
			NOSPLIT = 0x0004,
			NOSWAP = 0x0008,
			DETAIL = 0x0010};

int		get_gblname_len(sm_uc_ptr_t blk_base, sm_uc_ptr_t key_base);
int		get_key_len(sm_uc_ptr_t blk_base, sm_uc_ptr_t key_base);
int		get_cmpc(sm_uc_ptr_t first_key, sm_uc_ptr_t second_key);
enum cdb_sc 	read_record(int *rec_size_ptr, int *key_cmpc_ptr, int *key_len_ptr, sm_uc_ptr_t key,
				int level, sm_uc_ptr_t blk_base, sm_uc_ptr_t rec_base);
