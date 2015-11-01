/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "arit.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "collseq.h"
#include "copy.h"
#include "stringpool.h"
#include "do_xform.h"
#include "format_targ_key.h"

GBLREF gv_namehead	*gv_target;
GBLREF bool		transform;

static readonly unsigned char pos_code[100] =
{
	0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
	0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a,
	0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a,
	0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a,
	0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a,
	0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a,
	0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a,
	0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a,
	0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a,
	0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a
};

static readonly unsigned char neg_code[100] =
{
	0xfe, 0xfd, 0xfc, 0xfb, 0xfa, 0xf9, 0xf8, 0xf7, 0xf6, 0xf5,
	0xee, 0xed, 0xec, 0xeb, 0xea, 0xe9, 0xe8, 0xe7, 0xe6, 0xe5,
	0xde, 0xdd, 0xdc, 0xdb, 0xda, 0xd9, 0xd8, 0xd7, 0xd6, 0xd5,
	0xce, 0xcd, 0xcc, 0xcb, 0xca, 0xc9, 0xc8, 0xc7, 0xc6, 0xc5,
	0xbe, 0xbd, 0xbc, 0xbb, 0xba, 0xb9, 0xb8, 0xb7, 0xb6, 0xb5,
	0xae, 0xad, 0xac, 0xab, 0xaa, 0xa9, 0xa8, 0xa7, 0xa6, 0xa5,
	0x9e, 0x9d, 0x9c, 0x9b, 0x9a, 0x99, 0x98, 0x97, 0x96, 0x95,
	0x8e, 0x8d, 0x8c, 0x8b, 0x8a, 0x89, 0x88, 0x87, 0x86, 0x85,
	0x7e, 0x7d, 0x7c, 0x7b, 0x7a, 0x79, 0x78, 0x77, 0x76, 0x75,
	0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x69, 0x68, 0x67, 0x66, 0x65
};

unsigned char *mval2subsc(mval *in_val, gv_key *out_key)
{
	boolean_t	is_negative;
	unsigned char	buf1[MAX_KEY_SZ + 1], buff[MAX_ZWR_KEY_SZ], ch, *cvt_table, *end, *in_ptr, *out_ptr;
	unsigned char	*tm, temp_mantissa[NUM_DEC_DG_2L / 2 + 3];	/* Need 1 byte for each two digits.  Add 3 bytes slop */
	mstr		mstr_ch, mstr_buf1;
	int4		mt, mw, mx;
	uint4		mvt;	/* Local copy of mvtype, bit ands use a int4, so do conversion once */
	unsigned int	digs, exp_val;
	int		tmp_len;

	error_def(ERR_GVSUBOFLOW);
	error_def(ERR_NUMOFLOW);
	error_def(ERR_GVIS);

	out_ptr = out_key->base + out_key->end;
	if (transform && gv_target->nct)
	{
		MV_FORCE_STR(in_val);
		mvt = in_val->mvtype | MV_NUM_APPROX;
	} else
		mvt = (uint4)in_val->mvtype;
	if (!(mvt & (MV_NM | MV_NUM_APPROX)))
	{	/* Not currently in numeric form.  Is it cannonical? */
		if (nm_iscan(in_val))
		{	/* Yes, convert it to numeric */
			(void)s2n(in_val);
			mvt = in_val->mvtype;
			if (!(mvt & MV_NM))
				rts_error(VARLSTCNT(1) ERR_NUMOFLOW);
		} else
		{	/* No, not numeric.  Note the fact for future reference */
			mvt = in_val->mvtype |= MV_NUM_APPROX;
		}
	}
	if (mvt & MV_NUM_APPROX)
	{	/* It's a string */
		in_ptr = (unsigned char *)in_val->str.addr;
		tmp_len = in_val->str.len;
		if (transform && gv_target->collseq)
		{
			mstr_ch.len = tmp_len;
			mstr_ch.addr = (char *)in_ptr;
			mstr_buf1.len = MAX_KEY_SZ + 1;
			mstr_buf1.addr = (char *)buf1;
			do_xform(gv_target->collseq->xform, &mstr_ch, &mstr_buf1, &tmp_len);
			in_ptr = buf1;
		}
		if ((unsigned char *)((int)out_key->end + tmp_len + 3) > (unsigned char *)(out_key->top - MAX_NUM_SUBSC_LEN))
		{
			if (0 == (end = format_targ_key(buff, MAX_ZWR_KEY_SZ, out_key, TRUE)))
				end = &buff[MAX_ZWR_KEY_SZ - 1];
			rts_error(VARLSTCNT(6) ERR_GVSUBOFLOW, 0, ERR_GVIS, 2, end - buff, buff);
		}
		*out_ptr++ = STR_SUB_PREFIX;
		for(;  tmp_len > 0;  tmp_len--)
		{
			ch = *in_ptr++;
			if (ch <= 1)
			{
				*out_ptr++ = STR_SUB_ESCAPE;
				if (out_ptr - (int)out_key->base + tmp_len + 3
					> (unsigned char *)(out_key->top - MAX_NUM_SUBSC_LEN))
				{
					if (0 == (end = format_targ_key(buff, MAX_ZWR_KEY_SZ, out_key, TRUE)))
						end = &buff[MAX_ZWR_KEY_SZ - 1];
					rts_error(VARLSTCNT(6) ERR_GVSUBOFLOW, 0, ERR_GVIS, 2, end - buff, buff);
				}
				ch++;	/* promote character */
			}
			*out_ptr++ = ch;
		}
		goto FINI;
	}
	/* Its a number, is it an integer? */
	if (mvt & MV_INT)
	{	/* Yes, its an integer, convert it */
		is_negative = FALSE;
		cvt_table = pos_code;
		if ((mt = in_val->m[1]) < 0)
		{
			is_negative = TRUE;
			cvt_table = neg_code;
			mt = -mt;
		} else  if (0 == mt)
		{
			*out_ptr++ = SUBSCRIPT_ZERO;
			goto FINI;
		}
		if (mt < 10)
		{
			*out_ptr++ = is_negative ? ~(SUBSCRIPT_BIAS - 2) : (SUBSCRIPT_BIAS - 2);
			*out_ptr++ = cvt_table[mt * 10];
			goto FINISH_NUMBER;
		}
		if (mt < 100)
		{
			*out_ptr++ = is_negative ? ~(SUBSCRIPT_BIAS - 1) : (SUBSCRIPT_BIAS - 1);
			*out_ptr++ = cvt_table[mt];
			goto FINISH_NUMBER;
		}
		tm = temp_mantissa;
		if (mt < 1000)
		{
			exp_val = SUBSCRIPT_BIAS;
			goto ODD_INTEGER;
		}
		if (mt < 10000)
		{
			exp_val = SUBSCRIPT_BIAS + 1;
			goto EVEN_INTEGER;
		}
		if (mt < 100000)
		{
			exp_val = SUBSCRIPT_BIAS + 2;
			goto ODD_INTEGER;
		}
		if (mt < 1000000)
		{
			exp_val = SUBSCRIPT_BIAS + 3;
			goto EVEN_INTEGER;
		}
		if (mt < 10000000)
		{
			exp_val = SUBSCRIPT_BIAS + 4;
			goto ODD_INTEGER;
		}
		if (mt < 100000000)
		{
			exp_val = SUBSCRIPT_BIAS + 5;
			goto EVEN_INTEGER;
		}
		exp_val = SUBSCRIPT_BIAS + 6;
ODD_INTEGER:
		*out_ptr++ = is_negative ? ~(exp_val) : (exp_val);
		mw = mx = mt / 10;
		mw *= 10;
		mw = mt - mw;
		mt = mx;
		if (mw)
		{
			*tm++ = cvt_table[mw * 10];
			goto FINISH_INTEGERS;
		}
		goto KEEP_STRIPING;
EVEN_INTEGER:
		*out_ptr++ = is_negative ? ~(exp_val) : (exp_val);
KEEP_STRIPING:
		while (mt)
		{
			mw = mx = mt / 100;
			mw *= 100;
			mw = mt - mw;
			mt = mx;
			if (mw)
			{
				*tm++ = cvt_table[mw];
				break;
			}
		}
FINISH_INTEGERS:
		while (mt)
		{
			mw = mx = mt / 100;
			mw *= 100;
			mw = mt - mw;
			*tm++ = cvt_table[mw];
			mt = mx;
		}
		while (tm > temp_mantissa)
			*out_ptr++ = *--tm;
		goto FINISH_NUMBER;
	}
	/* Convert 18 digit number */
	cvt_table = pos_code;
	if (0 != (is_negative = in_val->sgn))
		cvt_table = neg_code;
	*out_ptr++ = is_negative ? ~(in_val->e - MV_XBIAS + SUBSCRIPT_BIAS) : (in_val->e - MV_XBIAS + SUBSCRIPT_BIAS);
	mt = in_val->m[1];
	mw = in_val->m[0];
	/* Strip top two digits */
	mx = mt / (MANT_HI / 100);
	*out_ptr++ = cvt_table[mx];
	mt = (mt - (mx * (MANT_HI / 100))) * 100;
	/*
	 * The two msd's have now been converted.  The maximum number of
	 * data remaining is 7 digits in "mt" and 9 digits in "mw".
	 * If mw is zero, then we should just grind out mt till we are done
	 *
	 */
	if (0 == mw)
		goto LAST_LONGWORD;
	/* there are more than 7 digits left.  First, we will put 8 digits in mt, (leaving 8 digits in mw) */
	mx = mw / (MANT_HI / 10);
	mt += mx * 10;
	mw = (mw - (mx * (MANT_HI / 10))) * 10;
	if (0 == mw)
		goto LAST_LONGWORD;
	for (digs = 0;  digs < 4;  digs++)
	{
		mx = mt / (MANT_HI / 100);
		*out_ptr++ = cvt_table[mx];
		mt = (mt - (mx * (MANT_HI / 100))) * 100;
	}
	mt = mw;
LAST_LONGWORD:
	while (mt)
	{
		mx = mt / (MANT_HI / 100);
		*out_ptr++ = cvt_table[mx];
		mt = (mt - (mx * (MANT_HI / 100))) * 100;
	}
FINISH_NUMBER:
	if (is_negative)
		*out_ptr++ = NEG_MNTSSA_END;
FINI:
	*out_ptr++ = KEY_DELIMITER;
	*out_ptr = KEY_DELIMITER;
	out_key->prev = out_key->end;
	out_key->end = out_ptr - out_key->base;
	if (0 >= (int)(out_key->top - out_key->end) - MAX_NUM_SUBSC_LEN)
	{	/* take care of extra space plus <NUL> terminator */
		if (0 == (end = format_targ_key(buff, MAX_ZWR_KEY_SZ, out_key, TRUE)))
			end = &buff[MAX_ZWR_KEY_SZ - 1];
		rts_error(VARLSTCNT(6) ERR_GVSUBOFLOW, 0, ERR_GVIS, 2, end - buff, buff);
	}
	return out_ptr;
}
