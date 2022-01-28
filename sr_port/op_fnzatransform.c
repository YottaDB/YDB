/****************************************************************
 *								*
 * Copyright (c) 2012-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2020-2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stdarg.h>
#include "gtm_string.h"
#include "gtm_stdio.h"

#include "gtmio.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "stringpool.h"
#include "collseq.h"
#include "do_xform.h"
#include "error.h"
#include "op.h"
#include "patcode.h"
#include "mvalconv.h"
#include "lv_val.h"
#include "alias.h"
#include "gtmimagename.h"
#include "format_targ_key.h"
#include "gtm_ctype.h"		/* for ISDIGIT_ASCII macro */
#include "gvn2gds.h"
#include "gvsub2str.h"
#include "io.h"
#include "gtm_descript.h"
#include "mv_stent.h"

#define MAX_KEY_SIZE	(MAX_KEY_SZ - 4)	/* internal and external maximums differ */

GBLREF gv_namehead	*gv_target;
GBLREF gv_namehead	*reset_gv_target;
GBLREF spdesc 		stringpool;
GBLREF mv_stent		*mv_chain;

LITREF mval		literal_null;

STATICDEF boolean_t	transform_direction;

static int zat_mprev_chn(unsigned char c);
static int zat_mprev_chs(unsigned char c);
static int zat_mnext_chn(unsigned char c);
static int zat_mnext_chs(unsigned char c);

error_def(ERR_COLLATIONUNDEF);
error_def(ERR_ZATRANSERR);
error_def(ERR_ZATRANSCOL);

CONDITION_HANDLER(op_fnzatransform_ch)
{
	START_CH(TRUE);
	RESET_GV_TARGET(DO_GVT_GVKEY_CHECK);
	if (transform_direction)
		DEBUG_ONLY(TREF(skip_mv_num_approx_assert) = FALSE);
	if (ERR_ZATRANSERR != SIGNAL)
	{	/* Override downstream GVSUBOFLOW error. This should not happen because we already
		 * limit the string lenth to less than the maximum key size.
		 */
		assert(FALSE);
		SIGNAL = ERR_ZATRANSERR;
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZATRANSERR);
	}
	NEXTCH;
}

/*
 * -----------------------------------------------
 * op_fnzatransform()
 * Converts between an MVAL and database internal representation (GDS) of the MVAL using a specified collation
 * sequence.
 *
 * Additional functionality returns the next or previous character in the collation sequence if requested.
 *
 * Arguments:
 *	msrc	 - Pointer to a string
 * 	col	 - Collation algorithm index
 * 	reverse	 - Specifies whether to
 *			Convert msrc from GVN to GDS representation (0)
 *			Convert from GDS to GVN representation (reverse) (1 and all other non-zero values except as below).
 *			Return the previous character in the collation sequence (-2)
 *			Return the next character in the collation sequence (2)
 * 	forceStr - Force convert the mval to a string. Use this to make numbers collate like strings
 * 	dst	 - The destination string containing the converted string.
 *
 * If reverse == 2 or -2, only the first character of any msrc is processed, and at most one dst character is returned.
 * -----------------------------------------------
 */
void op_fnzatransform(mval *msrc, int col, int reverse, int forceStr, mval *dst)
{
	gv_key		save_currkey[DBKEYALLOC(MAX_KEY_SZ)];
	gv_key		*gvkey;
	unsigned char	*key;
	unsigned char	buff[MAX_KEY_SZ + 1], msrcbuff[MAX_KEY_SZ + 1];
	collseq 	*csp;
	gv_namehead	temp_gv_target;
	mval		*src;
	mstr		opstr;
	boolean_t	coll_noxutil = 0;
	boolean_t	coll_failxutil = 0;
	unsigned char 	c;
	int		length;
	int		status;
	int		res;
	gtm32_descriptor outbuff1, insub1;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (0 != col)
	{
		csp = ready_collseq(col);
		if (NULL == csp)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_COLLATIONUNDEF, 1, col);
	} else
		csp = NULL; /* Do not issue COLLATIONUNDEF for 0 collation */
	MV_FORCE_DEFINED(msrc);	/* issue a LVUNDEF error if undefined */
	if (MV_IS_STRING(msrc) && (0 == msrc->str.len))
	{	/* Null string, return it back */
		*dst = *msrc;
		return;
	}
	/* Ensure global variables "gv_target" and "transform" are set as they are needed by
	 * mval2subsc/gvsub2str. "transform" is already set to TRUE (the correct value).
	 * Assert that. So need to only set "gv_target".
	 */
	assert(TREF(transform));
	assert(INVALID_GV_TARGET == reset_gv_target);
	reset_gv_target = gv_target;
	gv_target = &temp_gv_target;
	memset(gv_target, 0, SIZEOF(gv_namehead));
	gv_target->collseq = csp;
	gvkey = &save_currkey[0];
	gvkey->prev = 0;
	gvkey->top = DBKEYSIZE(MAX_KEY_SZ);
	gvkey->end = 0;
	/* Avoid changing the characteristics of the caller's mval. Protect value of caller's value even after potential
	 * stringpool garbage collections (which a local copy does not do. Note, we only need to pop the mv_stent we push
	 * onto the M stack on a normal return. Error returns always unwind the current M stack frame which will also
	 * unwind this mv_stent.
	 */
	PUSH_MV_STENT(MVST_MVAL);		/* Create a temporary on M stack */
	src = &mv_chain->mv_st_cont.mvs_mval;
	*src = *msrc;
	if (forceStr)
	{
		MV_FORCE_STR(src);
		src->mvtype |= MV_NUM_APPROX;	/* Force the mval to be treated as a string */
	}
	ESTABLISH(op_fnzatransform_ch);
	transform_direction = (0 == reverse);
	/* Previously the code relied on all non-zero values triggering reverse mapping.
	 * We now use a switch to catch explicit values (with a large default for the remaining non-zero cases).
	 */
	switch (reverse)
	{
		/* Forward mapping */
		case 0:
			/* convert to subscript format; mval2subsc returns NULL in place of GVSUBOFLOW */
			if (MAX_KEY_SIZE < src->str.len)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZATRANSERR);
			/* Setup false key in case a GVSUBOFLOW occurs where format_targ_key processes the key for printing */
			key = gvkey->base;
			*key++ = KEY_DELIMITER;
			gvkey->end++;
			DEBUG_ONLY(TREF(skip_mv_num_approx_assert) = TRUE);
			if (NULL == (key = mval2subsc(src, gvkey, TRUE)))
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZATRANSERR);
			COPY_ARG_TO_STRINGPOOL(dst, &gvkey->base[gvkey->end], &gvkey->base[1]);
			DEBUG_ONLY(TREF(skip_mv_num_approx_assert) = FALSE);
			break;
		/* Return the previous char in the collation sequence (or null string if at bottom already) */
		case -2:
			if (!gtm_utf8_mode)
			{
				/* Since we are not in UTF mode, all our characters *and* collations from
				 * the translation tables (real & notional) are defined to be one byte.
				 * Our arguments may be longer strings, but we only consider the first byte.
				 * For 'M' collation, return the previous byte code or no change for 0
				 */
				if (0 == col)
				{	/* M collation */
					c = src->str.addr[0];
					res = (forceStr) ? zat_mprev_chs(c) : zat_mprev_chn(c);
					/* If we can't go down, return null string */
					if (-1 == res)
						*dst = literal_null;
					else
					{
						c = (unsigned char)res;
						COPY_ARG_TO_STRINGPOOL(dst, (&c)+1, &c);
					}
				} else
				{	/* We are not using M collation.  */
					if (NULL == csp->xutil)
					{	/* We cannot continue without an xutil function */
						coll_noxutil = 1;
						break;
					}
					/* Map our args for the xutil callout */
					insub1.type = DSC_K_DTYPE_T;
					insub1.len = 1;
					insub1.val = src->str.addr;
					outbuff1.type = DSC_K_DTYPE_T;
					outbuff1.len = 1;
					outbuff1.val = &c;
					status = (csp->xutil)(&insub1, 1, &outbuff1, &length, 1, !forceStr);
					if (status)
					{
						coll_failxutil = 1;
						break;
					}
					if (0 < length)
					{
						COPY_ARG_TO_STRINGPOOL(dst, (&c)+1, &c);
					} else
						*dst = literal_null;
				}
			} else
				coll_failxutil = 1;	/*We do not support UTF-8 yet */
			break;
		/* Return the next char in the collation sequence (or null string if at top already) */
		case 2:
			if (!gtm_utf8_mode)
			{
				/*
				 * Since we are not in UTF mode, all our characters *and* collations from
				 * the translation tables (real & notional) are defined to be one byte.
				 * Our arguments may be longer strings, but we only consider the first byte.
				 */
				if (0 == col)	/* For 'M' collation, return the previous byte code or no change for 0 */
				{
					c = src->str.addr[0];
					res = (forceStr) ? zat_mnext_chs(c) : zat_mnext_chn(c);
					/* If we can't go up, return null string */
					if (-1 == res)
					{
						*dst = literal_null;
					} else
					{
						c = (unsigned char)res;
						COPY_ARG_TO_STRINGPOOL(dst, (&c)+1, &c);
					}
				} else
				{	/* We are not using M collation. */
					if (NULL == csp->xutil)		/* We cannot continue without an xutil function */
					{
						coll_noxutil = 1;
						break;
					}
					/* Map our args for the xutil callout */
					insub1.type = DSC_K_DTYPE_T;
					insub1.len = 1;
					insub1.val = src->str.addr;
					outbuff1.type = DSC_K_DTYPE_T;
					outbuff1.len = 1;
					outbuff1.val = &c;
					status = (csp->xutil)(&insub1, 1, &outbuff1, &length, 2, !forceStr);
					if (status)
					{
						coll_failxutil = 1;
						break;
					}
					if (0 < length)
					{
						COPY_ARG_TO_STRINGPOOL(dst, (&c)+1, &c);
					} else
						*dst = literal_null;
				}
			} else
				coll_failxutil = 1;	/* We do not support UTF-8 yet */
			break;
		/* Reverse mapping case (1)
		 * Note switch Fall Through!
		 * Note that all unclaimed, non-zero, third argument values default to
		 * the 'reverse' code.  This was the previous (undocumented) behavior.
		 * We are claiming certain values, above, which override this.
		 */
		case 1:
		default:
			/* convert back from subscript format; cannot exceed MAX_KEY_SZ for gvsub2str to work */
			if (MAX_KEY_SZ < src->str.len)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZATRANSERR);
			memset(msrcbuff, 0, MAX_KEY_SZ);		/* Ensure null termination */
			memcpy(msrcbuff, src->str.addr, src->str.len);
			opstr.addr = (char *)buff;
			opstr.len = MAX_KEY_SZ;
			if (opstr.addr == (char *)(key = gvsub2str((unsigned char *)msrcbuff, &opstr, FALSE)))
				dst = (mval *)&literal_null;
			else
				COPY_ARG_TO_STRINGPOOL(dst, key, &buff[0]);
			break;
	}
	/* Pop the mv_stent pointed to by "src" then restore gv_target */
	REVERT;
	POP_MV_STENT();
	RESET_GV_TARGET(DO_GVT_GVKEY_CHECK);
	/* Now that we have restored our state, if we failed due to no xutil helper, or xutil err: invoke an error */
	if (coll_failxutil)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZATRANSCOL);
	if (coll_noxutil)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_COLLATIONUNDEF, 1, col);
}

/* The following routines implement numeric aware
 * next & previous operations in the 'M'
 * collation in non-UTF-8 mode.  The code is based on
 * that of 'xutil' used in collation libraries, and considers the M
 * collation to be a 256 element identity matrix
 * (ie m[0x??] = 0x??).
 */

/* Get the previous character from the M collation
 * honoring the GT.M convention that numbers sort before
 * anything else.
 *
 * Parameters:
 *	c - character of interest
 *
 * Returns:
 *	The previously collating character or
 *	-1 if none exists.
 */
static int zat_mprev_chn(unsigned char c)
{
	int coll;
	int retval = -1;	/* default to none exists */

	/* If we are not a digit, then we return the
	 * next lower previously collating character
	 * that is not a digit.  If there is none,
	 * we return '9'
	 */
	if (!ISDIGIT_ASCII(c))
	{
		coll = c;
		switch (--coll)	/* If going down takes us into digits, skip over them to '/' */
		{
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				coll = '/';
			default:
				break;
		}
		retval = (-1 == coll) ? '9' : coll;
	} else
	{			/* we are a digit */
		if ('0' < c)
			retval = --c;
	}
	return retval;
}

/* Get the previous character from the collation
 * on a strictly table based approach (ie: The GT.M
 * numeric sorts before string convention is not used).
 *
 * Parameters:
 *	c - character of interest
 *
 * Returns:
 *	The previously collating character or
 *	-1 if none exist.
 */
static int zat_mprev_chs(unsigned char c)
{
	int coll;
	int retval = -1;	/* default to none exists */

	coll = c;
	if (coll)
	{
		--coll;
		retval = coll;
	}
	return retval;
}

/* Get the next character from the collation
 * honoring the GT.M convention that numbers sort before
 * strings.
 *
 * Parameters:
 *	c - character of interest
 *
 * Returns:
 *	The next collating character or
 *	-1 if none exists.
 */
static int zat_mnext_chn(unsigned char c)
{
	int coll;
	int retval = -1;	/* default to none exists */

	/*
	 * If we are not a digit, then we return the
	 * next higher collating character
	 * that is not a digit, if there is one.
	 */
	if (!ISDIGIT_ASCII(c))
	{
		coll = c;
		assert(256 > coll);
		if (255 > coll)
		{
			switch (++coll)		/* If we incremented into digits, skip to ':' */
			{
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
					coll = ':';
				default:
					break;
			}
			if (256 != coll)
				retval = coll;
		}
	} else
	{	/* we are a digit */
		retval = ('9' != c) ? c+1 : 0;
	}
	return retval;
}

/* Get the next character from the collation
 * on a strictly table based basis.
 *
 * Parameters:
 *	c - character of interest
 *
 * Returns:
 *	The next collating character or
 *	-1 if none exist.
 */
static int zat_mnext_chs(unsigned char c)
{
	int coll;
	int retval = -1;	/* default to none exists */

	coll = c;
	assert(256 > coll);
	if (255 > coll)
	{
		++coll;
		retval = coll;
	}
	return retval;
}
