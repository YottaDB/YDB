/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
 * -----------------------------------------------------
 * Convert a string subscript to MUMPS string
 * -----------------------------------------------------
 */

#include "mdef.h"

#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "copy.h"
#include "collseq.h"
#include "do_xform.h"
#include "gvsub2str.h"
#include "zshow.h"

#define LARGE_EXP	10000

GBLREF  gv_namehead     *gv_target;
LITREF	unsigned short	dpos[], dneg[];

error_def(ERR_GVSUBOFLOW);
/*
 * -----------------------------------------------------
 * Convert a string subscript to MUMPS string
 * Save result in a buffer pointed by targ.
 *
 * Entry:
 *	sub	- input string in subscript format
 *	targ	- output mstr value.
 *	xlat_flg- translate flag.
 *		  If true convert string to MUMPS format (aka ZWRITE format)
 * Return:
 *	(pointer to the last char.
 *	converted in the targ string) + 1.
 * -----------------------------------------------------
 */
unsigned char *gvsub2str(unsigned char *sub, mstr *opstr, boolean_t xlat_flg)
{
	unsigned char	buf[MAX_KEY_SZ + 1], buf1[MAX_KEY_SZ + 1], ch, *ptr, trail_ch, *str, *targ, *targ_end;
	unsigned short	*tbl_ptr;
	int		num, rev_num, trail_zero;
	span_subs	*subs_ptr;
	int		expon, in_length, targ_len;
	mstr		mstr_ch, mstr_targ;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	ch = *sub++;
	targ = (unsigned char *)opstr->addr;
	if (STR_SUB_PREFIX == ch || (SUBSCRIPT_STDCOL_NULL == ch && KEY_DELIMITER == *sub))
	{	/* If this is a string */
		in_length = 0;
		ptr = (xlat_flg ? buf : targ);
		while ((ch = *sub++))
		{	/* Copy string to ptr, xlating each char */
			in_length++;
			if (STR_SUB_ESCAPE == ch)	/* if this is an escape, demote next char */
				ch = (*sub++ - 1);
			*ptr++ = ch;
		}
		if (TREF(transform) && gv_target && gv_target->collseq)
		{
			mstr_ch.len = in_length;
			mstr_ch.addr = (char *)(xlat_flg ? buf : targ);
			mstr_targ.len = SIZEOF(buf1);
			mstr_targ.addr = (char *)buf1;
			do_xform(gv_target->collseq, XBACK, &mstr_ch, &mstr_targ, &targ_len);
			if (targ_len > opstr->len)
			{
				/* The only way we know of for targ_len to be greater than opstr->len is if the collation
				 * library allocated an external buffer greater than opstr->len
				 */
				assert(mstr_targ.addr != (char *)buf1);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_GVSUBOFLOW);
			}
			if (!xlat_flg)
			{
				memcpy(targ, mstr_targ.addr, targ_len);	/* mstr_targ.addr is used just in case it is
									 * reallocated by the XBACK routine.
									 */
				targ = targ + targ_len;
			} else
			{
				in_length = targ_len;
				ptr = (unsigned char *)mstr_targ.addr;	/* mstr_targ.addr is used just in case it is
									 * reallocated in the XBACK routine.
									 */
			}
		} else if (xlat_flg)
			ptr = &buf[0];
		else
			targ = ptr;
		if (xlat_flg)
		{
			format2zwr((sm_uc_ptr_t)ptr, in_length, targ, &targ_len);
			assert(targ_len <= opstr->len);
			targ = targ + targ_len;
		}
	} else
	{	/* Number */
		targ_end = targ + opstr->len;
		assert(opstr->len >= SPAN_PREFIX_LEN);
		if (SUBSCRIPT_ZERO == ch)
			*targ++ = '0';
		else if(SPANGLOB_SUB_ESCAPE == ch)
		{
			ASGN_SPAN_PREFIX(targ);
			targ += SPAN_PREFIX_LEN;
			subs_ptr = (span_subs *)(sub - 1);
			/* Internal to the database, the spanning node blocks counting starts with 0 i.e. first spanning
			 * node block has ID '0' but while displaying first block of spanning node is displayed as '1'
			 * Hence the below adjustment in the 'num'.
			 */
			num = SPAN_GVSUBS2INT(subs_ptr) + 1;
			sub = (sub - 1) + SPAN_SUBS_LEN;
			for (trail_zero = 0; (num % DECIMAL_BASE) == 0; trail_zero++, num /= DECIMAL_BASE)
				;
			for (rev_num = 0; num > 0; rev_num = (rev_num * DECIMAL_BASE + num % DECIMAL_BASE), num /= DECIMAL_BASE)
				;
			for (; (rev_num > 0) && (targ < targ_end);
				*targ++ = (rev_num % DECIMAL_BASE + ASCII_0), rev_num /= DECIMAL_BASE)
				;
			for (; (trail_zero > 0) && (targ < targ_end); *targ++ = '0', trail_zero--);
			if (*sub != 0)
				*targ++ = '*';
		}
		else
		{
			tbl_ptr = (unsigned short *)&dpos[0] - 1;
			trail_ch = KEY_DELIMITER;
			if ((0 <= (signed char)ch) && (targ < targ_end))
			{	/* Bit 7 of the exponent is set for positive numbers; must be negative */
				trail_ch = NEG_MNTSSA_END;
				tbl_ptr = (unsigned short *)dneg;
				ch = ~ch;
				*targ++ = '-';
			}
			ch -= (SUBSCRIPT_BIAS - 1);	/* Unbias the exponent */
			expon = ch;
			if (0 >= (signed char)ch)
			{	/* number is a fraction */
				ch = -(signed char)ch;
					/* Save decimal point */
				*targ++ = '.';
					/* generate leading 0's */
				do *targ++ = '0';
				while (((signed char)ch-- > 0) && (targ < targ_end))
					;
					/* make expon. really large to avoid
					 * generating extra dots */
				expon = LARGE_EXP;
			}
			while ((ch = *sub++) && (ch != trail_ch) && (targ < targ_end))
			{	/* Convert digits loop */
					/* adjust dcm. point */
				if (0 >= (expon -= 2))
				{
					if (0 != expon)
					{
						*targ++ = '.';
						expon = LARGE_EXP;
						PUT_USHORT(targ, tbl_ptr[ch]);
						targ += SIZEOF(short);
					} else
					{	/* Insert dot between digits */
						PUT_USHORT(targ, tbl_ptr[ch]);
						targ += SIZEOF(short);
						*targ = *(targ - 1);
						*(targ - 1) = '.';
						targ++;
						expon = LARGE_EXP;
					}
				} else
				{
					PUT_USHORT(targ, tbl_ptr[ch]);
					targ += SIZEOF(short);
				}
			}
			if ((LARGE_EXP - 100) < expon)
			{
				if ('0' == *(targ - 1))
					targ--;
				if ('.' == *(targ - 1))
					targ--;
			} else
				while (--expon > 0 && targ < targ_end)
					*targ++ = '0';
		}
	}
	return targ;
}
