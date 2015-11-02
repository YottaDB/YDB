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

#include "mdef.h"
#include "gtm_string.h"
#include "mlkdef.h"

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "gtmctype.h"
#include "cli.h"
#include "dse.h"
#include "gvsub2str.h"
#include "gdsblk.h"
#include "zshow.h"

#define COUNT_TRAILING_ZERO(NUM, WCP, TRAIL_ZERO)					\
{											\
	if (0 == NUM)									\
		*WCP++ = '0';								\
	for (TRAIL_ZERO = 0; (NUM > 0) && (0 == (NUM % 10)); TRAIL_ZERO++, NUM /= 10)	\
		;									\
}

#define	OUTPUT_NUMBER(NUM, WCP, TRAIL_ZERO)						\
{											\
	for (rev_num = 0; NUM > 0; rev_num = (rev_num * 10 + NUM % 10), NUM /= 10)	\
			;								\
	for (; rev_num > 0; *WCP++ = (rev_num % 10 + ASCII_0), rev_num /= 10)		\
			;								\
	for (; TRAIL_ZERO > 0 ; *WCP++ = '0', TRAIL_ZERO--)				\
			;								\
}

GBLREF enum dse_fmt	dse_dmp_format;
GBLREF gd_region	*gv_cur_region;
GBLREF char	 	patch_comp_key[MAX_KEY_SZ + 1];

static unsigned char	*work_buff;
static unsigned int	work_buff_length;

boolean_t dse_fdmp(sm_uc_ptr_t data, int len)
{
	unsigned char	*key_char_ptr, *work_char_ptr;
	int 		dest_len;
	unsigned char	*ret_addr;
	boolean_t	is_snblk=FALSE;
	span_subs	*ss_ptr;		/*spanning node key pointer */
	unsigned int	snbid, offset, trail_zero, rev_num, num;
	unsigned short	blk_sz;

	if (work_buff_length < ZWR_EXP_RATIO(gv_cur_region->max_rec_size))
	{
		work_buff_length = ZWR_EXP_RATIO(gv_cur_region->max_rec_size);
		if (work_buff)
			free (work_buff);
		work_buff = (unsigned char *)malloc(work_buff_length);
	}
	work_char_ptr = work_buff;
	*work_char_ptr++ = '^';
	for (key_char_ptr = (uchar_ptr_t)patch_comp_key; *key_char_ptr ; key_char_ptr++)
	{
		if (PRINTABLE(*key_char_ptr))
			*work_char_ptr++ = *key_char_ptr;
		else
			return FALSE;
	}
	key_char_ptr++;
	if (SPAN_START_BYTE != *key_char_ptr) /*Global has subscript*/
	{
		*work_char_ptr++ = '(';
		for (;;)
		{
			work_char_ptr = gvsub2str(key_char_ptr, work_char_ptr, TRUE);
			/* Removed unnecessary checks for printable characters (PRINTABLE()) here
			 * since the data being written into files (OPENed files) would have been
			 * passed through ZWR translation which would have taken care of converting
			 * to $CHAR() or $ZCHAR() */

			for (; *key_char_ptr ; key_char_ptr++)
				;
			key_char_ptr++;
			/* Check if this is spanning node if yes break out of the loop */
			if (SPAN_START_BYTE == *key_char_ptr
			    && (int)*(key_char_ptr + 1) >= SPAN_BYTE_MIN
			    && (int)*(key_char_ptr + 2) >= SPAN_BYTE_MIN)
			{
				is_snblk = TRUE;
				break;
			}
			if (*key_char_ptr)
				*work_char_ptr++ = ',';
			else
				break;
		}
		*work_char_ptr++ = ')';
	} else	/*Spanning node without subscript*/
		is_snblk = TRUE;
	if (is_snblk)
	{
		ss_ptr = (span_subs *)key_char_ptr;
		snbid = SPAN_GVSUBS2INT(ss_ptr);
		key_char_ptr = key_char_ptr + SPAN_SUBS_LEN + 1; /* Move out of special subscript of spanning node */
		blk_sz = gv_cur_region->dyn.addr->blk_size;
		/* Decide the offset of the content of a block inside the value of spanning node*/
		offset = (snbid) ? (blk_sz - (SIZEOF(blk_hdr) + SIZEOF(rec_hdr) + gv_cur_region->dyn.addr->reserved_bytes
				    + (key_char_ptr - (uchar_ptr_t)patch_comp_key + 1))) * (snbid - 1) : 0 ;
		ret_addr =(unsigned char *)memmove((void *)(work_buff+4), (void *)work_buff, (work_char_ptr - work_buff));
		assert(*ret_addr == '^');
		*work_buff = '$';
		*(work_buff + 1) = 'z';
		*(work_buff + 2) = 'e';
		*(work_buff + 3) = '(';
		/* length of "$ze(" is 4, so move the work_char_ptr by 4*/
		work_char_ptr = work_char_ptr + 4;
		*work_char_ptr++ = ',';

		/* Dump the offset of the content of a block inside the value of spanning node */
		num = snbid ? offset : 0;
		COUNT_TRAILING_ZERO(num, work_char_ptr, trail_zero);
		num = offset;
		OUTPUT_NUMBER(num, work_char_ptr, trail_zero);
		*work_char_ptr++ = ',';

		/* Dump the length of the content of a block */
		num = snbid ? len : 0;
		COUNT_TRAILING_ZERO(num, work_char_ptr, trail_zero);
		num = snbid ? len : 0;
		OUTPUT_NUMBER(num, work_char_ptr, trail_zero);
		*work_char_ptr++ = ')';
	}
	assert(MAX_ZWR_KEY_SZ >= work_char_ptr - work_buff);
	if (GLO_FMT == dse_dmp_format)
	{
		if (!dse_fdmp_output(work_buff, (int4)(work_char_ptr - work_buff)))
			return FALSE;
		if (!dse_fdmp_output(data, len))
			return FALSE;
	} else
	{
		assert(ZWR_FMT == dse_dmp_format);
		*work_char_ptr++ = '=';
		if(is_snblk && !snbid)
		{
			*work_char_ptr++ = '"';
			*work_char_ptr++ = '"';
			dest_len = 0;
		} else
			format2zwr(data, len, work_char_ptr, &dest_len);
		if (!dse_fdmp_output(work_buff, (int4)(work_char_ptr + dest_len - work_buff)))
			return FALSE;
	}
	return TRUE;
}
