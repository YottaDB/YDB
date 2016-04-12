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

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stat.h"
#include "gtm_facility.h"
#include "gtm_stdlib.h"
#include "gtm_limits.h"
#include "gtm_unistd.h"

#include <errno.h>
#include <glob.h>
#include <libgen.h>

#include "error.h"
#include "parse_file.h"
#include "eintr_wrappers.h"
#include "lv_val.h"
#include "stringpool.h"
#include "op.h"
#include "mvalconv.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "have_crit.h"
#include "op_fnzsearch.h"

LITREF		mval	literal_null;

STATICFNDCL	int	pop_top(lv_val *src, mval *res);

error_def(ERR_INVSTRLEN);
error_def(ERR_MEMORY);
error_def(ERR_ZSRCHSTRMCT);

/* This routine is invoked on $ZSEARCH() and ZRUPDATE commands as well as when compiling a source file (via compile_source_file()).
 * The main purpose of the routine is to return the full path to a file that corresponds to the specified pattern. The pattern may
 * be absolute (starting with '/') or relative and may include wildcard characters ('*' and '?' for multi- and single-character
 * replacements) and environment variables.
 *
 * To traverse all files matching the specified pattern, in collating sequence, the function may be invoked consecutively with the
 * same argument. Once the list of matching files is exhaused (or if the query did not yield any results), an empty string is
 * returned. The implementation relies on a local M variable, referenced by the fnzsearch_lv_vars global, to store the results of
 * the first invocation with a new pattern; subsequent invocations use the pop_top() function to $ORDER() to, and KILL, the first
 * found entry.
 *
 * The function supports 256 individual search "streams," allowing to maintain results of various searches independently. Each
 * stream is identified by an integer in the range of [0; 255]. Negative numbers have been adopted for internal callers to avoid
 * interference with user-initiated searches.
 *
 * Parameters:
 *   pattern   - search pattern, such as 'a.*', '/etc/lib*.?', or 'file-1'.
 *   indx      - search stream number, between 0 and 255, inclusive.
 *   mfunc     - indication of whether the caller is M code or an internal function, 0 being the latter.
 *   ret       - full path to the first matching file in collating sequence.
 *
 * Returns: an integer encoded in plength format that contains in each of its bytes the length of one of the matching entry's
 *          characteristics: length of the directory path, length of the (file) name, and length of the extension. For more details,
 *          refer to parse_file.h.
 */
int op_fnzsearch(mval *pattern, mint indx, mint mfunc, mval *ret)
{
	plength		pret;
	char		pblk_buf[GTM_PATH_MAX], sanitized_buf[GTM_PATH_MAX];
	char		*match, *buf_ptr;
	int		i, status, length;
	mval		file;
	parse_blk	pblk;
	lv_val		*var_ref;
	plength		*match_len;
	glob_t		globbuf;
	boolean_t	absolute;
	intrpt_state_t	prev_intrpt_state;
#ifdef _AIX
	boolean_t	use_stat;
	struct		stat statbuf;
#endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (mfunc && ((MAX_STRM_CT <= indx) || (0 > indx)))	/* Allow an out-of-range stream only if used internally. */
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZSRCHSTRMCT);
	ESTABLISH_RET(fnzsrch_ch, -1);
	TREF(fnzsearch_nullsubs_sav) = TREF(lv_null_subs);
	TREF(lv_null_subs) = LVNULLSUBS_OK;			/* $ZSearch processing depends on this. */
	MV_FORCE_STR(pattern);
	if (MAX_FBUFF < pattern->str.len)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_INVSTRLEN, 2, pattern->str.len, MAX_FBUFF);
	MV_FORCE_MVAL(((mval *)TADR(fnzsearch_sub_mval)), indx);
	TREF(fnzsearch_lv_vars) = op_srchindx(VARLSTCNT(2) TREF(zsearch_var), (mval *)TADR(fnzsearch_sub_mval));
	if (TREF(fnzsearch_lv_vars))
	{	/* If the parameter is different, kill the local with previous results. */
		assert((TREF(fnzsearch_lv_vars))->v.mvtype & MV_STR);
		if ((pattern->str.len != (TREF(fnzsearch_lv_vars))->v.str.len)
			|| memcmp(pattern->str.addr, (TREF(fnzsearch_lv_vars))->v.str.addr, pattern->str.len))
		{
			op_kill(TREF(fnzsearch_lv_vars));
			TREF(fnzsearch_lv_vars) = NULL;
		}
	}
	ret->mvtype = MV_STR;
	if ((0 != pattern->str.len) && !TREF(fnzsearch_lv_vars))
	{
		memset(&pblk, 0, SIZEOF(pblk));
		pblk.buffer = pblk_buf;
		pblk.buff_size = MAX_FBUFF;
		if (parse_file(&pattern->str, &pblk) & 1)
		{	/* Establish new search context. */
			TREF(fnzsearch_lv_vars) = op_putindx(VARLSTCNT(2) TREF(zsearch_var), TADR(fnzsearch_sub_mval));
			(TREF(fnzsearch_lv_vars))->v = *pattern;	/* zsearch_var(indx)=original spec */
			if (0 != pblk.b_esl)
			{	/* Create a NULL-terminated buffer with the pattern to be passed to glob(). If we are dealing with a
				 * relative-path pattern, prepend it with the absolute path of the working directory first.
				 */
				if ('/' != pblk_buf[0])
				{
					if (NULL == getcwd(sanitized_buf, ARRAYSIZE(sanitized_buf)))
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) errno);
					else
					{
						length = STRLEN(sanitized_buf);
						if (MAX_FBUFF < length + 1 + pblk.b_esl)
							rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_INVSTRLEN,
									2, length + 1 + pblk.b_esl, MAX_FBUFF);
						sanitized_buf[length] = '/';
					}
					buf_ptr = sanitized_buf + length + 1;
				} else
					buf_ptr = sanitized_buf;
				/* Escape '[' and ']' for glob() processing (because $zsearch() does not support such sets). Make
				 * sure that we have enough storage room (the string includes the length of our working directory,
				 * if prepended, the escaped pattern, which could be twice as long as the original, and a trailing
				 * '\0' character).
				 */
				assert(ARRAYSIZE(sanitized_buf) >= (buf_ptr - sanitized_buf) + 2 * pblk.b_esl + 1);
				pblk_buf[pblk.b_esl] = '\0';
				ESCAPE_BRACKETS(pblk_buf, buf_ptr);
				/* Canonicalize the path by appropriately removing '.' and '..' path modifiers. */
				CANONICALIZE_PATH(buf_ptr);
				/* Do not sort the matches because we use $order() to obtain them from a local anyway. */
				DEFER_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
#ifdef _AIX
				use_stat = !((pblk.fnb & (1 << V_WILD_NAME)) || (pblk.fnb & (1 << V_WILD_DIR)));
				if (use_stat)
				{
					STAT_FILE(sanitized_buf, &statbuf, status);
					if (!status)
						globbuf.gl_pathc = 1;
				}
				else
#endif
					status = glob(sanitized_buf, LINUX_ONLY(GLOB_PERIOD | ) GLOB_NOSORT,
							(int (*)(const char *, int))NULL, &globbuf);
				ENABLE_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
				if (0 == status)
				{
					TREF(fnzsearch_globbuf_ptr) = &globbuf;
					file.mvtype = MV_STR;
					for (i = 0; i < globbuf.gl_pathc; i++)
					{	/* We do not care for . and .. */
#ifdef _AIX
						if (use_stat)
							match = sanitized_buf;
						else
#endif
							match = globbuf.gl_pathv[i];
						length = STRLEN(match);
						if ((length > 1) && ('.' == match[length - 1]) && (('/' == match[length - 2])
								|| ((length > 2) && ('.' == match[length - 2])
								&& ('/' == match[length - 3]))))
							continue;
						/* If the resolved length is too long to be used in a local, skip it. */
						if (MAX_FBUFF < length)
							continue;
						ENSURE_STP_FREE_SPACE(length);
						file.str.addr = match;
						file.str.len = length;
						s2pool(&file.str);
						var_ref = op_putindx(VARLSTCNT(2) TREF(fnzsearch_lv_vars), &file);
						var_ref->v.mvtype = MV_STR;
						var_ref->v.str.len = 0;
						match_len = (plength *)&(var_ref->v.m[1]);
						SET_LENGTHS(match_len, file.str.addr, length, TRUE);
					}
#ifdef _AIX
					if (!use_stat)
					{
#endif
						DEFER_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
						globfree(&globbuf);
						ENABLE_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
						TREF(fnzsearch_globbuf_ptr) = NULL;
#ifdef _AIX
					}
#endif
				} else
				{
#ifdef _AIX
					if (!use_stat)
					{
#endif
						DEFER_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
						globfree(&globbuf);
						ENABLE_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
						TREF(fnzsearch_globbuf_ptr) = NULL;
						if (GLOB_NOSPACE == status)
							rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ENOMEM); /* Ran out of memory. */
						else if (GLOB_ABORTED == status)
							rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) EACCES); /* Access error. */
						else
							assert(GLOB_NOMATCH == status);			  /* No matches found. */
#ifdef _AIX
					}
#endif
				}
			}
		}
	}
	/* If we have placed something into a local (now or in a prior invocation), obtain it. */
	if (TREF(fnzsearch_lv_vars))
		pret.p.pint = pop_top(TREF(fnzsearch_lv_vars), ret);
	else
	{
		ret->str.len = 0;
		pret.p.pint = 0;
	}
	assert((0 == ret->str.len) || (pret.p.pblk.b_esl == ret->str.len));
	TREF(lv_null_subs) = TREF(fnzsearch_nullsubs_sav);
	REVERT;
	return pret.p.pint;
}

/* Condition handler for the op_fnzsearch() operation. It takes care of restoring the lv_null_subs value and freeing the glob()
 * buffer, if necessary.
 */
STATICFNDEF CONDITION_HANDLER(fnzsrch_ch)
{
	START_CH(TRUE);
	/* START_CH() defines prev_intrpt_state */
	if (NULL != TREF(fnzsearch_globbuf_ptr))
	{
		DEFER_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
		globfree(TREF(fnzsearch_globbuf_ptr));
		ENABLE_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
		TREF(fnzsearch_globbuf_ptr) = NULL;
	}
	TREF(lv_null_subs) = TREF(fnzsearch_nullsubs_sav);
	NEXTCH;
}

/* This routine clears the cached search results on a particular "stream."
 *
 * Parameters:
 *   indx - search stream number, between 0 and 255, inclusive.
 */
void zsrch_clr(int indx)
{
	lv_val	*tmp;
	mval	x;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_MVAL(&x, indx);
	tmp = op_srchindx(VARLSTCNT(2) TREF(zsearch_var), &x);
	op_kill(tmp);
}

/* This routine returns the value stored at the first subscript (in collating sequence) of the passed M local. It also sets one of
 * the passed arguments to the string containing the subscript. Its main use is to aid op_fnzsearch() in traversing through the list
 * of previously cached search results for a particular pattern. The function also verifies that the file it returns still exists.
 * If not, it skips it.
 *
 * Parameters:
 *   src - pointer to the local that is to be $ORDER()ed.
 *   res - pointer to an mval where the first valid subscript is to be placed.
 *
 * Returns: the value stored at the first subscript in collating sequence. It is an integer encoded in plength format that contains
 *          in each of its bytes the length of one of the matching entry's characteristics: length of the directory path, length of
 *          the (file) name, and length of the extension. For more details, refer to parse_file.h.
 */
STATICFNDEF int pop_top(lv_val *src, mval *res)
{
	lv_val		*tmp;
	plength		pret;
	struct stat	statbuf;
	int		stat_res;
	char		file_name[MAX_FBUFF + 1];

	while (TRUE)
	{
		op_fnorder(src, (mval *)&literal_null, res);
		if (res->str.len)
		{
			tmp = op_getindx(VARLSTCNT(2) src, res);
			assert(MAX_FBUFF >= res->str.len);
			pret.p.pint = tmp->v.m[1];
			op_kill(tmp);	/* Remove this element from the tree. */
			memcpy(file_name, res->str.addr, res->str.len);
			file_name[res->str.len] = '\0';
			STAT_FILE(file_name, &statbuf, stat_res);
			if (-1 == stat_res)
			{
				if (ENOENT != errno)
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) errno);
				continue;
			}
		} else
		{
			pret.p.pint = 0;
			op_kill(src);
		}
		break;
	}
	return pret.p.pint;
}
