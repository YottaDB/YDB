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

#ifndef GDSBLK_H_INCLUDED
#define GDSBLK_H_INCLUDED

/* gdsblk.h */

#include <sys/types.h>
#include "gdsdbver.h"

#define BML_LEVL ((unsigned char)(~0x0))

#define CST_BSIZ(b)		((b)->bsiz - SIZEOF(blk_hdr))
#define CST_RSIZ(r)		((r)->rsiz - SIZEOF(rec_hdr))
#define CST_TOR(r)		((uchar_ptr_t)(r) + (r)->rsiz)
#define CST_TOB(b)		((uchar_ptr_t)(b) + (b)->bsiz)
#define CST_RIB(r,b)		((uchar_ptr_t)(r) + (r)->rsiz <= CST_TOB(b))
#define CST_1ST_REC(b)		((rec_hdr_ptr_t)((uchar_ptr_t)(b) + SIZEOF(blk_hdr)))
#define CST_NXT_REC(r)		((uchar_ptr_t)(r) + (r)->rsiz)
#define CST_BOK(r)		((uchar_ptr_t)(r) + SIZEOF(rec_hdr))
#define CST_USAR(b, r)		((b)->bsiz - ((uchar_ptr_t(r) + (r)->rsiz - (uchar_ptr_t)(b)))
#define CST_KSIZ		(gv_curr_key->end - gv_curr_key->base + 1)
#define IS_LEAF(b)		(0 == ((blk_hdr_ptr_t)(b))->levl)
#define IS_BML(b)		(BML_LEVL == ((blk_hdr_ptr_t)(b))->levl)
#define GAC_RSIZE(rsize,r,tob)	if ((uchar_ptr_t)(r) + ((rsize) = ((rec_hdr_ptr_t)(r))->rsiz) > tob) return(-1)
#define MIN_DATA_SIZE		1 + 2	/*    1 byte of key + 2 nulls for terminator     */
#define MAX_EXTN_COUNT		1048575
#define MIN_EXTN_COUNT		0
#define STDB_ALLOC_MAX		8388607	/* Based on JNL_ALLOC_MAX */
#define STDB_ALLOC_MIN		128	/* 128 Blocks */
#define	MAX_DB_BLK_SIZE		((1 << 16) - 512)	/* 64Kb - 512 (- 512 to take care of VMS's max I/O capabilities) */

/* Note: EVAL_CMPC not to be confused with the previously existing GET_CMPC macro in mu_reorg.h!
 * The maximum key size in V5 was 255 bytes, including the two null bytes at the end. Two distinct keys
 * could have a common prefix of at most 252.The 255 limit was imposed because the record header was
 * 3 bytes and had only 1 byte for compression count. But on UNIX the record header is actually 4 bytes
 * (see pragmas below), leaving an unused filler byte. To accommodate larger keys while
 * maintaining compatibility with V5, we can overload the previously unused values of cmpc
 * to get compression counts of up to 1020, which will support keys up to 1023 bytes.
 * As in V5 compression counts of between 0 and 252, inclusive, are stored in the first byte.
 * rp->cmpc values of 253, 254, and 255 indicate information is stored in the second byte.
 *
 * There are several constants used below. They are very specific and ONLY used here. We don't #define them
 * because that implies you can change one and the logic will still work. But that's not the case.
 * 253		--- cmpc value just beyond the highest possible in V5 (252). 253 = OLD_MAX_KEY_SZ - two terminators.
 * 256		--- Number of distinct compression counts that can be stored when cmpc is either 253, 254, or 255.
 * 		    Corresponds to the one extra byte, which can store 256 values.
 * 0x03FF	--- Equals 1023. We want to avoid having EVAL_CMPC return huge values due to concurrent changes
 * 		    to block contents. This limits the result, preventing unintentionally large memcpy's in e.g. gvcst_put.
 */
#ifdef UNIX
# define EVAL_CMPC(RP)		((253 > (RP)->cmpc)							\
					? (RP)->cmpc							\
					: (253 + ((RP)->cmpc - 253) * 256 + (int)(RP)->cmpc2) & 0x03FF)
# define EVAL_CMPC2(RP, VAR)										\
{	/* Usage note: make sure VAR is an int */							\
	VAR = (RP)->cmpc;										\
	if (253 <= VAR)											\
		VAR = (253 + ((VAR) - 253) * 256 + (int)(RP)->cmpc2) & 0x03FF;				\
}
# define SET_CMPC(RP, VAL)										\
{													\
	int lcl_cmpc_val;										\
													\
	lcl_cmpc_val = (VAL);										\
	if (253 > lcl_cmpc_val)										\
		(RP)->cmpc = (unsigned char)lcl_cmpc_val;						\
	else												\
	{												\
		(RP)->cmpc = (unsigned char)(253 + (lcl_cmpc_val - 253) / 256);				\
		(RP)->cmpc2 = (unsigned char)((lcl_cmpc_val - 253) & 0xFF);				\
	}												\
}
#else
# define EVAL_CMPC(RP)		((int)(RP)->cmpc)
# define EVAL_CMPC2(RP, VAR)										\
{	/* Usage note: make sure VAR is an int */							\
	VAR = (RP)->cmpc;										\
}
# define SET_CMPC(RP, VAL)										\
{													\
	(RP)->cmpc = (unsigned char)(VAL);								\
}
#endif

/* The following macro picks up a record from a block using PREC as the pointer to the record and validates
 * that the record length, NRECLEN, meets base criteria (is not 0 and does not exceed the top of the
 * block as identified by PTOP) and return NBLKID on the assumption the block is an index block.
 * LONG_BLK_ID tells whether the block that PREC is part of uses 32 or 64 bit block_ids.
 */
#define GET_AND_CHECK_RECLEN(STATUS, NRECLEN, PREC, PTOP, NBLKID, LONG_BLK_ID)	\
{										\
	sm_uc_ptr_t	PVAL;							\
										\
	STATUS = cdb_sc_normal;							\
	GET_USHORT(NRECLEN, &((rec_hdr_ptr_t)PREC)->rsiz);			\
	if (NRECLEN == 0)							\
		STATUS = cdb_sc_badoffset;					\
	else if ((PREC + NRECLEN) > PTOP)					\
		STATUS = cdb_sc_blklenerr;					\
	else									\
	{									\
		PVAL = PREC + NRECLEN - SIZEOF_BLK_ID(LONG_BLK_ID);		\
		READ_BLK_ID(LONG_BLK_ID, &NBLKID, PVAL);			\
	}									\
}

/* The following macro picks up a the level from a block using PBLKBASE as the pointer to the header and validates
 * that the block level, NLEVL, meets base criteria (is not greater than MAX_BT_DEPTH and matches the expected
 * level as identified by DESIREDLVL)
 */
#define GET_AND_CHECK_LEVL(STATUS, NLEVL, DESIREDLVL, PBLKBASE)	\
{									\
	STATUS = cdb_sc_normal;						\
	NLEVL = ((blk_hdr_ptr_t)PBLKBASE)->levl;			\
	if (MAX_BT_DEPTH <= (int)NLEVL)					\
		STATUS = cdb_sc_maxlvl;					\
	else if (ANY_ROOT_LEVL == DESIREDLVL)				\
	{								\
		if (0 == (int)NLEVL)					\
			STATUS = cdb_sc_badlvl;				\
	} else if (DESIREDLVL !=(int)NLEVL)				\
		STATUS = cdb_sc_badlvl;					\
}

#if defined(__alpha) && defined(__vms)
# pragma member_alignment save
# pragma nomember_alignment
#endif

/* Version 4 block header */
typedef struct v15_blk_hdr_struct
{
	unsigned short	bsiz;		/* block size */
	unsigned char	levl;		/* block level. level 0 is data level. level 1 is
					 * first index level. etc.
					 */
	uint4		tn;		/* transaction number when block was written */
} v15_blk_hdr;

/* Current block header */
typedef struct blk_hdr_struct
{
	unsigned short	bver;		/* block version - overlays V4 block size */
	unsigned char	filler;
	unsigned char	levl;		/* block level. level 0 is data level. level 1 is
					 * first index level. etc.
					 */
	unsigned int	bsiz;		/* number of currently used bytes in the block */
	trans_num	tn;		/* transaction number when block was written */
} blk_hdr;

typedef struct rec_hdr_struct
{
	unsigned short	rsiz;		/* size of the record in bytes */
	unsigned char	cmpc;		/* compression count of the record that allows for values up to 252
					 * (See EVAL_CMPC comments at beginning of file for explanation)
					 */
#	ifdef UNIX
	unsigned char	cmpc2;		/* extra byte allows compression count up to 1020 */
#	endif
} rec_hdr;

#if defined(__alpha) && defined(__vms)
# pragma member_alignment restore
#endif

/* Define pointer types to above structures */
#ifdef DB64
# ifdef __osf__
#  pragma pointer_size(save)
#  pragma pointer_size(long)
# else
#  error UNSUPPORTED PLATFORM
# endif
#endif

typedef v15_blk_hdr *v15_blk_hdr_ptr_t;	/* From jnl format 15 used in last GT.M V4 version */
typedef blk_hdr *blk_hdr_ptr_t;
typedef rec_hdr *rec_hdr_ptr_t;

#ifdef DB64
# ifdef __osf__
#  pragma pointer_size(restore)
# endif
#endif

/* Macros/Inline functions for bstar records */
#define BSTAR_REC_SIZE_32	INTCAST((SIZEOF(rec_hdr) + SIZEOF(block_id_32)))
#define BSTAR_REC_SIZE_64	INTCAST((SIZEOF(rec_hdr) + SIZEOF(block_id_64)))
/* Comment the below macro definition out as the availability of this macro can cause subtle bugs.
 * See https://gitlab.com/YottaDB/DB/YDBTest/-/work_items/522#note_1489467002 for an example.
 * Not having this macro will force the caller to use the "bstar_rec_size" static inline function
 * which is the right usage because the star-record size varies depending on whether the block format is V6 or V7.
 *
 * #define BSTAR_REC_SIZE		BSTAR_REC_SIZE_64
 */

static inline int bstar_rec_size(boolean_t long_blk_id)
{
	return (long_blk_id ? BSTAR_REC_SIZE_64 : BSTAR_REC_SIZE_32);
}

static inline void bstar_rec(sm_uc_ptr_t r, boolean_t long_blk_id)
{
	((rec_hdr_ptr_t)r)->rsiz = (unsigned short)bstar_rec_size(long_blk_id);
	SET_CMPC((rec_hdr_ptr_t)r, 0);
}

#define MAX_RESERVE_B(X, Y) ((X)->blk_size - (X)->max_key_size - SIZEOF(blk_hdr) - SIZEOF(rec_hdr) \
        - SIZEOF_BLK_ID(Y) - bstar_rec_size(Y)) /* anything past key can span */
#define CHKRECLEN(r,b,n) ((unsigned int)((n) + (uchar_ptr_t)(r) - (uchar_ptr_t)(b)) <= (unsigned int)((blk_hdr_ptr_t)(b))->bsiz)

/*********************************************************************
      read record size from REC_BASE (temp_ushort must be defined)
 *********************************************************************/
#define GET_RSIZ(REC_SIZE, REC_BASE)                                    \
        GET_USHORT(temp_ushort, &(((rec_hdr_ptr_t)(REC_BASE))->rsiz));  \
        REC_SIZE = temp_ushort

int4 bm_find_blk(int4 hint, sm_uc_ptr_t base_addr, int4 total_bits, boolean_t *used);
void bm_setmap(block_id bml, block_id blk, int4 busy);
void bml_newmap(blk_hdr_ptr_t ptr, uint4 size, trans_num curr_tn, enum db_ver ondsk_blkver);

/* End of gdsblk.h */

#endif
