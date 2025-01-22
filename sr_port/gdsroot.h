/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2023 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GDSROOT_H
#define GDSROOT_H

#include "copy.h"
#include <sys/types.h>

#define DIR_ROOT 1
#define CDB_STAGNATE 3
#define CDB_MAX_TRIES (CDB_STAGNATE + 2) /* used in defining arrays, usage requires it must be at least 1 more than CSB_STAGNATE*/
#define T_FAIL_HIST_DBG_SIZE 32
#define MAX_BT_DEPTH 11
#define CSE_LEVEL_DRT_LVL0_FREE (MAX_BT_DEPTH + 2) /* used to indicate a level-0 block in GV tree will be freed */
#define GLO_NAME_MAXLEN 33	/* 1 for length, 4 for prefix, 15 for dvi, 1 for $, 12 for fid */
#define MAX_NUM_SUBSC_LEN 10	/* one for exponent, nine for the 18 significant digits */
/* Define padding in a gv_key structure to hold
 *	1) A number : This way we are ensured a key buffer overflow will never occur while converting a number
 *		from mval representation to subscript representation.
 *	2) 16 byte of a string : This way if we are about to overflow the max-key-size, we are more likely to fit the
 *		overflowing key in the padding space and so can give a more user-friendly GVSUBOFLOW message which
 *		includes the overflowing subscript in most practical situations.
 */
#define	MAX_GVKEY_PADDING_LEN	(MAX_NUM_SUBSC_LEN + 16)
#define EXTEND_WARNING_FACTOR	3
/* Define macro to compute the maximum key size required in the gv_key structure based on the database's maximum key size.
 * Align it to 4-byte boundary as this macro is mostly used by targ_alloc which allocates 3 keys one for gv_target->clue,
 * one for gv_target->first_rec and one for gv_target->last_rec. The alignment ensures all 3 fields start at aligned boundary.
 * In the following macro, we can ideally use the ROUND_UP2 macro but since this is used in a typedef (of "key_cum_value")
 * in gdscc.h, we cannot use that macro as it contains GTMASSERT expressions to do the 2-power check. To avoid this,	[BYPASSOK]
 * we use the ROUND_UP macro (which has no checks). It is ok to do that instead of the more-efficient ROUND_UP2 macro
 * as the second parameter is a constant so all this should get evaluated at compile-time itself.
 */
#define DBKEYSIZE(KSIZE)	(ROUND_UP((KSIZE + MAX_GVKEY_PADDING_LEN), 4))

/* Possible states for TREF(in_mu_swap_root_state) (part of MUPIP REORG -TRUNCATE) */
#define MUSWP_NONE		0	/* default; not in mu_swap_root */
#define MUSWP_INCR_ROOT_CYCLE	1	/* moving a root block; need to increment root_search_cycle */
#define MUSWP_FREE_BLK		2	/* freeing a directory block; need to write leaf blocks to snapshot file */
#define MUSWP_DIRECTORY_SWAP	3	/* moving a directory block; just checked by cert_blk */

typedef gtm_uint64_t	trans_num;
typedef uint4		trans_num_4byte;

typedef int4		block_id_32;	/* block_id type used pre-V7 kept for compatibility with old DBs
					 * allows for GDS block #s to have 32 bits but see GDS_MAX_BLK_BITS below
					 */
typedef uint4		ublock_id_32;	/* unsigned type of the same length as block_id_32 */
typedef gtm_int8	block_id_64;	/* block_id type used in V7+
					 * allows for GDS block #s to have 64 bits but see GDS_MAX_BLK_BITS below
					 */
typedef gtm_uint8	ublock_id_64;	/* unsigned type of the same length as block_id_64 */
typedef block_id_64	block_id;	/* default block id type used in current release
					 * NOTE: This definition is duplicated in rc_cpt_ops.h
					 * 	and if changed should be changed there as well
					 * NOTE: GDEVERIF.m assumes block_id to be int8.
					 * NOTE: GBLDEF.m assumes block_id to be int8.
					 */
typedef ublock_id_64	ublock_id;	/* unsigned type of the same length as the default block_id in the current release */

#define BLK_NUM_64BIT			/* Enables 64-bit block ID handling in DSE */

/* Returns size of block_id type based on result of IS_64_BLK_ID (See gdsdbver.h) */
#define SIZEOF_BLK_ID(X)	((X) ? SIZEOF(block_id_64) : SIZEOF(block_id_32))

/* These are memory access macros relabeled for explicit block_id references */
#define PUT_BLK_ID_32(X,Y)			\
{						\
	assert((block_id_32)(Y) == (Y));	\
	PUT_LONG(X,(block_id_32)Y);		\
}
#define GET_BLK_ID_32(X,Y)	GET_LONG(X,Y)
#define GET_BLK_ID_32P(X,Y)	GET_LONGP(X,Y)
#define PUT_BLK_ID_64(X,Y)	PUT_LLONG(X,Y)
#define GET_BLK_ID_64(X,Y)	GET_LLONG(X,Y)
#define GET_BLK_ID_64P(X,Y)	GET_LLONGP(X,Y)

/* These are memory access macros for general case block_id references
 * instead of a specific block_id width
 */
#define PUT_BLK_ID(X,Y)		PUT_BLK_ID_64(X,Y)
#define GET_BLK_ID(X,Y)		GET_BLK_ID_64(X,Y)
#define GET_BLK_IDP(X,Y)	GET_BLK_ID_64P(X,Y)

static inline void WRITE_BLK_ID(boolean_t long_blk_id, block_id blkid, sm_uc_ptr_t ptr)
{
	if (long_blk_id)
		PUT_BLK_ID_64(ptr, blkid);
	else
		PUT_BLK_ID_32(ptr, blkid);
}

static inline void READ_BLK_ID(boolean_t long_blk_id, block_id* blkid, sm_uc_ptr_t ptr)
{
	if (long_blk_id)
		GET_BLK_ID_64(*blkid, ptr);
	else
		GET_BLK_ID_32(*blkid, ptr);
}

/* This is the byte swap macro used for endian changing relabeled for block_id references */
#define BLK_ID_32_BYTESWAP(X)	GTM_BYTESWAP_32(X)
#define BLK_ID_64_BYTESWAP(X)	GTM_BYTESWAP_64(X)
#define BLK_ID_BYTESWAP(X)	BLK_ID_64_BYTESWAP(X)

#define GDS_MAX_BLK_BITS	62	/* see blk_ident structure in gdskill.h for why this cannot be any greater */
#define GDS_MAX_VALID_BLK	(0x3FFFFFFFFFFFFFFF)	/* max valid block # in a GT.M database, ((1 << GDS_MAX_BLK_BITS) - 1) */
#define GDS_CREATE_BLK_MAX	(block_id)(-1)	/* i.e. 0xFFFFFFFFFFFFFFFF which also has 63rd bit set to 1 indicating
						 * it is a created block
						 */
#define V6_GDS_CREATE_BLK_MAX	(block_id_32)(-1)

enum db_acc_method
{	dba_rms,
	dba_bg,
	dba_mm,
	dba_cm,
	dba_usr,
	n_dba,
	dba_dummy = 0x0ffffff	/* to ensure an int on S390 */
};

#define CREATE_IN_PROGRESS n_dba

/* the following definitions are warped by history, but cleanup is fraught because they are embedded in files */
typedef struct
{
	char dvi[16];
	unsigned short did[3];
	unsigned short fid[3];
} gds_file_id;		/* VMS artifact baked into things that would need care to change */

/* Note the below is not the same size on all platforms but must be less than or equal to gds_file_id */
typedef struct gd_id_struct
{
	ino_t	inode;
	dev_t	device;
} unix_file_id;

typedef unix_file_id	gd_id;

/* 2 replaces st_gen, an unused OSF1 artifact anticipating file gens; GTM64_ONLY 2 replaces previously implicit filler */
#define UNIQUE_ID_SIZE SIZEOF(gd_id) + ((GTM64_ONLY(4) NON_GTM64_ONLY(2)) * SIZEOF(char))

typedef union
{
	gd_id 		uid;
	char		file_id[UNIQUE_ID_SIZE];
} unique_file_id;


/* Since it is possible that a block_id/unix_file_id/gd_id may live in shared memory, define a
 * shared memory pointer type to it so the pointer will be 64 bits if necessary.
 */

#ifdef DB64
# ifdef __osf__
#  pragma pointer_size(save)
#  pragma pointer_size(long)
# else
#  error UNSUPPORTED PLATFORM
# endif
#endif

typedef block_id	*block_id_ptr_t;
typedef volatile block_id	*v_block_id_ptr_t;
typedef unix_file_id	*unix_file_id_ptr_t;
typedef gd_id		*gd_id_ptr_t;

#ifdef DB64
# ifdef __osf__
#  pragma pointer_size(restore)
# endif
#endif

typedef struct vms_lock_sb_struct
{
	short	cond;
	short	reserved;
	int4	lockid;
	int4	valblk[4];
} vms_lock_sb;

typedef struct
{
	uint4	low, high;
} date_time;

#define gdid_cmp(A, B) 							\
	(((A)->inode != (B)->inode)					\
		? ((A)->inode > (B)->inode ? 1 : -1)			\
		: ((A)->device != (B)->device) 				\
			? ((A)->device > (B)->device ? 1 : -1)		\
			: 0)

#define is_gdid_gdid_identical(A, B) (0 == gdid_cmp(A, B) ? TRUE: FALSE)

#define	VALFIRSTCHAR(X)			(ISALPHA_ASCII(X) || ('%' == X))
#define	VALFIRSTCHAR_WITH_TRIG(X)	(ISALPHA_ASCII(X) || ('%' == X) GTMTRIG_ONLY(|| (HASHT_GBL_CHAR1 == X)))
#define	VALKEY(X)			(ISALPHA_ASCII(X) || ISDIGIT_ASCII(X))

/* Prototypes below */
block_id get_dir_root(void);
boolean_t get_full_path(char *orig_fn, unsigned int orig_len, char *full_fn, unsigned int *full_len,
										unsigned int max_len, uint4 *status);
void gvinit(void);
#endif
