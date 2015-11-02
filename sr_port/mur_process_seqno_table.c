/****************************************************************
 *								*
 *	Copyright 2003, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "min_max.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"

#ifdef INT8_NATIVE
#	define ALL_BITS_IN_WORD_SET(X)	(MAXUINT8 == *(gtm_uint64_t *)(X))
#	define IS_PTR_WORD_ALIGNED(X)	IS_PTR_8BYTE_ALIGNED(X)
#	define NATIVE_PTR_TYPE		gtm_uint64_t
#else
#	define ALL_BITS_IN_WORD_SET(X)	(MAXUINT4 == *(uint4 *)(X))
#	define IS_PTR_WORD_ALIGNED(X)	IS_PTR_4BYTE_ALIGNED(X)
#	define NATIVE_PTR_TYPE		uint4
#endif

GBLREF mur_gbls_t	murgbl;
LITREF char		first_zerobit_position[256];
DEBUG_ONLY(GBLREF mur_opt_struct	mur_options;)

void mur_process_seqno_table(seq_num *min_broken_seqno, seq_num *losttn_seqno)
{
	size_t		seq_arr_size, index, seqno_span, byte, offset;
	jnl_tm_t	min_time;
	seq_num		min_brkn_seqno, min_resolve_seqno, max_resolve_seqno, lcl_losttn_seqno, stop_rlbk_seqno;
	unsigned char	*seq_arr, bit;
	multi_struct	*multi;
	ht_ent_int8	*curent, *topent;
	NATIVE_PTR_TYPE	*ptr, *ptr_top;

	assert(mur_options.rollback);
	min_resolve_seqno = min_brkn_seqno = MAXUINT8;
	max_resolve_seqno = 0;
	for (curent = murgbl.token_table.base, topent = murgbl.token_table.top; curent < topent; curent++)
	{
		if ((HTENT_VALID_INT8(curent, multi_struct, multi)) && NULL != multi)
		{
			if (multi->token < min_resolve_seqno)
				min_resolve_seqno = multi->token;
			if (multi->token > max_resolve_seqno)
				max_resolve_seqno = multi->token;
			if (0 < multi->partner && multi->token < min_brkn_seqno)
				min_brkn_seqno = multi->token;	/* actually sequence number */
			assert(NULL == (multi_struct *)multi->next);
		}
	}
	lcl_losttn_seqno = *losttn_seqno;
	if (murgbl.resync_seqno)
		stop_rlbk_seqno = murgbl.resync_seqno;	/* do not process after resync_seqno */
	else
		stop_rlbk_seqno = MAXUINT8;/* allow default rollback to continue forward processing till last valid record */
	assert(lcl_losttn_seqno <= min_resolve_seqno); /* Usually it will be ==; but it can be < as found in C9D11-002465 */
	/* If the losttn seqno is equal to the min_resolve_seqno, then determine the first seqno that is missing (gap) from
	 * min_resolve_seqno to max_resolve_seqno. Since this involves some computation, avoid this if we know for sure
	 * the losttn_seqno cannot eventually lie in the (min,max) range. This is possible if one of stop_rlbk_seqno or
	 * min_brkn_seqno is lesser than the min_resolve_seqno (in this case that will be the eventual value of losttn_seqno).
	 */
	if ((lcl_losttn_seqno >= min_resolve_seqno)
		&& (stop_rlbk_seqno >= min_resolve_seqno) && (min_brkn_seqno >= min_resolve_seqno))
	{	/* Update losttn_seqno to the first seqno gap from min_resolve_seqno to max_resolve_seqno */
		assert(max_resolve_seqno >= min_resolve_seqno);
		seqno_span = (max_resolve_seqno - min_resolve_seqno + 1);
		seq_arr_size = DIVIDE_ROUND_UP(seqno_span, 8); /* Need only an 8th of the actual memory since we use bit-array */
		seq_arr = (uchar_ptr_t) malloc(seq_arr_size);
		ptr = (NATIVE_PTR_TYPE *)(seq_arr);
		ptr_top = ptr + seq_arr_size;
		memset(seq_arr, 0, seq_arr_size);
		/* The below for-loop sets the BIT corresponding to a sequence number (as an offset from the min_resolve_seqno) */
		for (curent = murgbl.token_table.base, topent = murgbl.token_table.top; curent < topent; curent++)
		{
			if ((HTENT_VALID_INT8(curent, multi_struct, multi)) && NULL != multi)
			{
				assert((multi->token >= min_resolve_seqno) && (multi->token <= max_resolve_seqno));
				index = multi->token - min_resolve_seqno;
				byte = index >> 3;
				bit = (char)(index & 7);
				assert(byte < seq_arr_size);
				seq_arr[byte] |= (1 << bit);
			}
		}
		/* Now that we have all the bits set according to the sequence numbers, we have to determine the first bit in this
		 * bit-array that is 0 and the offset of that bit position from min_resolve_seqno is our losttn_seqno. Any bits
		 * that are set AFTER this are considered as broken sequence numbers. Instead of going through the bit-array one by
		 * one, we do the following optimization:
		 * 1 - We use the underlying word size of the platform and use that to read in a 8-byte or 4-byte value at a given
		 * offset and see if it is equal to the maximum 8-byte or maximum 4-byte unsigned value of that platform. If so,
		 * we are guaranteed that all the bits in that 64-bit or 32-bit sequence are 1. This way, we process the bit-array
		 * in chunks of 8-byte or 4-byte entites.
		 * 2 - At the end of Step 1, we are left with less than 8 or 4 byte array values that are unprocessed. We run
		 * another for-loop skipping all the bytes that are 0xFF
		 * 3 - At the end of Step 2, we are left with the first byte that is NOT 0xFF. Use the pre-defined array containing
		 * first zero-bit positions for 0x00 to 0xFE.
		 */
		for (; ptr <= ptr_top - 1; ptr++)
		{	/* Step 1 */
			assert(IS_PTR_WORD_ALIGNED(ptr));
			if (ALL_BITS_IN_WORD_SET(ptr))
				continue;
			break;
		}
		assert(IS_PTR_WORD_ALIGNED(ptr));
		index = (uchar_ptr_t)(ptr) - &seq_arr[0];
		for (; (index < seq_arr_size) && (0xFF == seq_arr[index]); index++); /* Step 2 */
		offset = index * 8;
		if (index < seq_arr_size)
			offset += first_zerobit_position[seq_arr[index]]; /* Step 3 */
		free(seq_arr);
		lcl_losttn_seqno = min_resolve_seqno + offset;
		/* Assert that losttn_seqno is within the range of min and max resolve sequence numbers. However, if all
		 * the sequence numbers in the range [min_resolve_seqno, max_resolve_seqno] are found in the hash table,
		 * then losttn_seqno is max_resolve_seqno + 1. Adjust assert accordingly.
		 */
		assert((lcl_losttn_seqno >= min_resolve_seqno) && (lcl_losttn_seqno <= (max_resolve_seqno + 1)));
	}
	if (lcl_losttn_seqno > stop_rlbk_seqno)
		lcl_losttn_seqno = stop_rlbk_seqno;
	if (lcl_losttn_seqno > min_brkn_seqno)
		lcl_losttn_seqno = min_brkn_seqno;
	*losttn_seqno = lcl_losttn_seqno;
	*min_broken_seqno = min_brkn_seqno;
	mur_multi_rehash();	/* To release memory and shorten the table */
	return;
}
