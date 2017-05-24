/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "is_canonic_name.h"
#include "zshow.h"
#include "io.h"

GBLREF gd_region	*gv_cur_region;
GBLREF gv_namehead	*gv_target;
GBLREF gv_namehead	*reset_gv_target;

error_def(ERR_COLLATIONUNDEF);
error_def(ERR_NOCANONICNAME);

STATICDEF boolean_t	save_transform;
STATICDEF gd_region	*save_gv_cur_region;

/* Restore global variables "gv_cur_region", "gv_target" and "transform" */
#define	RESTORE_GBL_VARS_BEFORE_FUN_RETURN										\
MBSTART {														\
	/* Restore global variables "gv_cur_region", "gv_target" and "transform" back to their original state */	\
	gv_cur_region = save_gv_cur_region;										\
	RESET_GV_TARGET(DO_GVT_GVKEY_CHECK);										\
	TREF(transform) = save_transform;										\
} MBEND

CONDITION_HANDLER(gvn2gds_ch)
{
	START_CH(TRUE);
	RESTORE_GBL_VARS_BEFORE_FUN_RETURN;
	NEXTCH;
}

#define MAX_LEN_FOR_CHAR_FUNC 6

boolean_t convert_key_to_db(mval *gvn, int start, int stop, gv_key *gvkey, unsigned char **key);

/*
 * -----------------------------------------------
 * gvn2gds()
 * Converts a global variable name (GVN) into its internal database repesentation (GDS)
 *
 * Arguments:
 *	gvn	      - Pointer to Source Name string mval. Must be in GVN form.
 *	buf	      - Pointer to a buffer large enough to fit the whole GDS of the passed in GVN.
 *	col	      - Collation number.
 * Return:
 *	unsigned char - Pointer to the end of the GDS written to gvkey.
 * -----------------------------------------------
 */
unsigned char *gvn2gds(mval *gvn, gv_key *gvkey, int act)
{
	boolean_t	est_first_pass, retn;
	collseq 	*csp;
	gd_region	tmpreg;
	gv_namehead	temp_gv_target;
	unsigned char 	*key, *key_top, *key_start;
	int		subscript, i, contains_env;
	int		*start, *stop;
	gv_name_and_subscripts 	start_buff, stop_buff;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* determine which buffer to use */
	DETERMINE_BUFFER(gvn, start_buff, stop_buff, start, stop);
	if (0 != act)
	{
		csp = ready_collseq(act);
		if (NULL == csp)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_COLLATIONUNDEF, 1, act);
	} else
		csp = NULL;	/* Do not issue COLLATIONUNDEF for 0 collation */
	retn = TRUE;
	assert(MV_IS_STRING(gvn));
	key_start = &gvkey->base[0];
	key = key_start;
	gvkey->prev = 0;
	gvkey->top = DBKEYSIZE(MAX_KEY_SZ);
	key_top = key_start + gvkey->top;
	/* We will parse all of the components up front. */
	if (!parse_gv_name_and_subscripts(gvn, &subscript, start, stop, &contains_env))
		NOCANONICNAME_ERROR(gvn);
	if (stop[contains_env] - start[contains_env] > gvkey->top)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_GVSUBOFLOW);
 	memcpy(key, gvn->str.addr + start[contains_env], stop[contains_env] - start[contains_env]);
	key += stop[contains_env] - start[contains_env];
	*key++ = KEY_DELIMITER;
	gvkey->end = key - key_start;
	/* Temporarily repoint global variables "gv_cur_region", "gv_target" and "transform".
	 * They are needed by mval2subsc for the following
	 *	"transform", "gv_target->nct", "gv_target->collseq" and "gv_cur_region->std_null_coll"
	 * Note that transform is usually ON, specifying that collation transformation is "enabled",
	 * and is only shut off for minor periods when something is being critically formatted (like
	 * we're doing here). Note that mval2subsc could issue an rts_error, so we establish a
	 * condition handler to restore the above.
	 */
	save_transform = TREF(transform);
	assert(save_transform);
	TREF(transform) = TRUE;
	reset_gv_target = gv_target;
	gv_target = &temp_gv_target;
	memset(gv_target, 0, SIZEOF(gv_namehead));
	gv_target->collseq = csp;
	memset(&tmpreg, 0, SIZEOF(gd_region));
	/* Assign "gv_cur_region" only after tmpreg has been fully initialized or timer interrupts can look at inconsistent copy */
	save_gv_cur_region = gv_cur_region;
	gv_cur_region = &tmpreg;
	gv_cur_region->std_null_coll = TRUE;
	ESTABLISH_NORET(gvn2gds_ch, est_first_pass);
	/* we know the number of subscripts, so we convert them all */
	for (i = 1 + contains_env; i <= contains_env + subscript; ++i)
	{
		if (!(retn = convert_key_to_db(gvn, start[i], stop[i], gvkey, &key)))
			break;
	}
	REVERT;
	RESTORE_GBL_VARS_BEFORE_FUN_RETURN;
	if (!retn || !CAN_APPEND_HIDDEN_SUBS(gvkey))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_GVSUBOFLOW);
	*key++ = KEY_DELIMITER;	/* add double terminating null byte */
	assert(key <= key_top);
	return key;
}

/* given the bounds of a particular subscript (assumed correct), we convert the subscript into
 * a form that mimics the GDS representation of that subscript
 */
boolean_t convert_key_to_db(mval *gvn, int start, int stop, gv_key *gvkey, unsigned char **key)
{
	mval 		tmpval, *mvptr, dollarcharmval;
	int 		isrc;
	char		strbuff[MAX_KEY_SZ + 1], *str, *str_top;
	char 		fnname[MAX_LEN_FOR_CHAR_FUNC], *c;
	boolean_t	is_zchar;
	int4		num;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (ISDIGIT_ASCII(gvn->str.addr[start]) ||
		'-' == gvn->str.addr[start] || '+' == gvn->str.addr[start] || '.' == gvn->str.addr[start])
	{	/* convert a number */
		tmpval.str.addr = &gvn->str.addr[start];
		tmpval.str.len 	= stop - start;
		tmpval.mvtype = MV_STR;
		mvptr = &tmpval;
		MV_FORCE_NUM(mvptr);
		if (MVTYPE_IS_NUM_APPROX(tmpval.mvtype))
			return FALSE;
		mval2subsc(&tmpval, gvkey, gv_cur_region->std_null_coll);
	} else
	{	/* It's a string. We need to accept strings, $CHAR args, and $ZCHAR args. */
		str = &strbuff[0];
		str_top = &strbuff[0] + MAX_KEY_SZ + 1;
		/* MV_NUM_APPROX needed by mval2subsc to skip val_iscan call */
		tmpval.mvtype = (MV_STR | MV_NUM_APPROX);
		for (isrc = start; isrc < stop; )
		{
			if ('_' == gvn->str.addr[isrc])
			{	/* We can skip this case, since we're already "appending"
				 * the strings on the lhs to the string on the rhs. */
				isrc++;
			} else if ('$' == gvn->str.addr[isrc])
			{	/* We determine if what comes after is a Char or a ZCHar,
				 * and copy over accordingly */
				c = &fnname[0];
				isrc++; /* skip the '$' */
				while ('(' != gvn->str.addr[isrc])
					*c++ = TOUPPER(gvn->str.addr[isrc++]);
				*c = '\0';
				assert(strlen(c) <= MAX_LEN_FOR_CHAR_FUNC - 1);
				if (!MEMCMP_LIT(fnname, "ZCHAR") || !MEMCMP_LIT(fnname, "ZCH"))
					is_zchar = TRUE;
				else if (!MEMCMP_LIT(fnname, "CHAR") || !MEMCMP_LIT(fnname, "C"))
					is_zchar = FALSE;
				else
					assert(FALSE);
				/* Parse the arguments */
				isrc++; /* skip the '(' */
				while (TRUE)
				{	/* Inside the argument list for $[Z]CHAR */
					/* STRTOUL will stop at the ',' or ')' */
					num = (int4)STRTOUL(&gvn->str.addr[isrc], NULL, 10);
#					ifdef UNICODE_SUPPORTED
					if (!is_zchar && is_gtm_chset_utf8)
						op_fnchar(2, &dollarcharmval, num);
					else
#					endif
						op_fnzchar(2, &dollarcharmval, num);
					assert(MV_IS_STRING(&dollarcharmval));
					if (dollarcharmval.str.len)
					{
						if (str + dollarcharmval.str.len > str_top)
							/* String overflows capacity. */
							return FALSE;
						memcpy(str, dollarcharmval.str.addr, dollarcharmval.str.len);
						str += dollarcharmval.str.len;
					}
					/* move on to the next argument */
					while (',' != gvn->str.addr[isrc] && ')' != gvn->str.addr[isrc])
						isrc++;
					if (',' == gvn->str.addr[isrc])
						isrc++;
					else
					{
						assert(')' == gvn->str.addr[isrc]);
						isrc++; /* skip ')' */
						break;
					}
				}
			} else if ('"' == gvn->str.addr[isrc])
			{	/* Assume valid string. */
				isrc++;
				while (isrc < stop && !('"' == gvn->str.addr[isrc] && '"' != gvn->str.addr[isrc+1]))
				{
					if (str == str_top)
						/* String overflows capacity. */
						return FALSE;
					if ('"' == gvn->str.addr[isrc] && '"' == gvn->str.addr[isrc+1])
					{
						*str++ = '"';
						isrc += 2;
					} else
						*str++ = gvn->str.addr[isrc++];
				}
				isrc++; /* skip over '"' */
			} else
				assert(FALSE);
		}
		tmpval.str.addr = strbuff;
		tmpval.str.len 	= str - strbuff;
		DEBUG_ONLY(TREF(skip_mv_num_approx_assert) = TRUE;)
		mval2subsc(&tmpval, gvkey, gv_cur_region->std_null_coll);
		DEBUG_ONLY(TREF(skip_mv_num_approx_assert) = FALSE;)
	}
	assert(gvkey->end < gvkey->top); /* else GVSUBOFLOW error would have been issued */
	*key = &gvkey->base[gvkey->end];
	return TRUE;
}

/*
 * -----------------------------------------------
 * gds2gvn()
 * Converts a key in internal database representation form to a global variable name (GVN).
 *
 * Arguments:
 *	gds	      - Pointer to Source Name string mval. Must be in GDS form.
 *	buf	      - Pointer to a buffer large enough to fit the whole GVN of the passed in GDS.
 *	col	      - Collation number.
 * Return:
 *	unsigned char - Pointer to the end of the GVN written to buff.
 * -----------------------------------------------
 */
unsigned char *gds2gvn(mval *gds, unsigned char *buff, int col)
{
	collseq 	*csp;
	unsigned char 	*key;
	gv_key 		save_currkey[DBKEYALLOC(MAX_KEY_SZ)];
	gv_key 		*gvkey;
	gd_region	tmpreg, *save_gv_cur_region;
	gv_namehead	temp_gv_target;
	boolean_t	est_first_pass;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	key = &buff[0];
	if (0 != col)
	{
		csp = ready_collseq(col);
		if (NULL == csp)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_COLLATIONUNDEF, 1, col);
	} else
		csp = NULL; /* Do not issue COLLATIONUNDEF for 0 collation */
	/* Temporarily repoint global variables "gv_target" and "transform".
	 * They are needed by format_targ_key/gvsub2str "transform" and "gv_target->collseq".
	 * Note that transform is usually ON, specifying that collation transformation is "enabled",
	 * and is only shut off for minor periods when something is being critically formatted (like
	 * we're doing here). While there should be no need for a condition handler, there is a
	 * a possible rts_error from gvsub2str in format_target_key, so we establish one.
	 */
	save_transform = TREF(transform);
	assert(save_transform);
	TREF(transform) = TRUE;
	reset_gv_target = gv_target;
	gv_target = &temp_gv_target;
	memset(gv_target, 0, SIZEOF(gv_namehead));
	gv_target->collseq = csp;
	assert(MV_IS_STRING(gds));
	gvkey = &save_currkey[0];
	gvkey->prev = 0;
	gvkey->top = DBKEYSIZE(MAX_KEY_SZ);
	if ((gvkey->top < gds->str.len) || (2 > gds->str.len)
			|| (KEY_DELIMITER != gds->str.addr[gds->str.len-1])
			|| (KEY_DELIMITER != gds->str.addr[gds->str.len-2]))
		*key++ = '\0';
	else
	{
		memcpy(&gvkey->base[0], gds->str.addr, gds->str.len);
		gvkey->end = gds->str.len - 1;
		ESTABLISH_NORET(gvn2gds_ch, est_first_pass);	/* format_targ_key calls gvsub2str which has an rts_error */
		if (0 == (key = format_targ_key(&buff[0], MAX_ZWR_KEY_SZ, gvkey, FALSE)))
			key = &buff[MAX_ZWR_KEY_SZ - 1];
		REVERT;
	}
	/* Restore global variables "gv_target" and "transform" back to their original state */
	RESET_GV_TARGET(DO_GVT_GVKEY_CHECK);
	TREF(transform) = save_transform;
	return key;
}
