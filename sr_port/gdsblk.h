/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __GDSBLK_H__
#define __GDSBLK_H__

/* gdsblk.h */

#include <sys/types.h>

#define BML_LEVL ((unsigned char)-1)

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
#define BSTAR_REC_SIZE		INTCAST((SIZEOF(rec_hdr) + SIZEOF(block_id)))
#define BSTAR_REC(r)		((rec_hdr_ptr_t)(r))->rsiz = BSTAR_REC_SIZE; SET_CMPC((rec_hdr_ptr_t)(r), 0);
#define IS_LEAF(b)		(0 == ((blk_hdr_ptr_t)(b))->levl)
#define IS_BML(b)		(BML_LEVL == ((blk_hdr_ptr_t)(b))->levl)
#define IS_BSTAR_REC(r)		((r)->rsiz == BSTAR_REC_SIZE)
#define GAC_RSIZE(rsize,r,tob)	if ((uchar_ptr_t)(r) + ((rsize) = ((rec_hdr_ptr_t)(r))->rsiz) > tob) return(-1)
#define MIN_DATA_SIZE           1 + 2     /*    1 byte of key + 2 nulls for terminator     */
#define MAX_EXTN_COUNT          65535
#define MIN_EXTN_COUNT          0
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

#if defined(__alpha) && defined(__vms)
# pragma member_alignment save
# pragma nomember_alignment
#endif

/* Version 4 block header */
typedef struct
{
	unsigned short	bsiz;		/* block size */
	unsigned char	levl;		/* block level. level 0 is data level. level 1 is
					 * first index level. etc.
					 */
	uint4		tn;		/* transaction number when block was written */
} v15_blk_hdr;

/* Current block header */
typedef struct
{
	unsigned short	bver;		/* block version - overlays V4 block size */
	unsigned char	filler;
	unsigned char	levl;		/* block level. level 0 is data level. level 1 is
					 * first index level. etc.
					 */
	unsigned int	bsiz;		/* block size */
	trans_num	tn;		/* transaction number when block was written */
} blk_hdr;

typedef struct
{
	unsigned short	rsiz;
	unsigned char	cmpc;
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

#define MAX_RESERVE_B(X) ((X)->blk_size - (X)->max_rec_size - SIZEOF(blk_hdr))
#define CHKRECLEN(r,b,n) ((unsigned int)((n) + (uchar_ptr_t)(r) - (uchar_ptr_t)(b)) <= (unsigned int)((blk_hdr_ptr_t)(b))->bsiz)

/*********************************************************************
      read record size from REC_BASE (temp_ushort must be defined)
 *********************************************************************/
#define GET_RSIZ(REC_SIZE, REC_BASE)                                    \
        GET_USHORT(temp_ushort, &(((rec_hdr_ptr_t)(REC_BASE))->rsiz));  \
        REC_SIZE = temp_ushort

int4 bm_find_blk(int4 hint, sm_uc_ptr_t base_addr, int4 total_bits, boolean_t *used);
void bm_setmap(block_id bml, block_id blk, int4 busy);
void bml_newmap(blk_hdr_ptr_t ptr, uint4 size, trans_num curr_tn);

/* End of gdsblk.h */

#endif
