/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_dirent.h"

#include <errno.h>
#include "gtm_stat.h"

#include "io.h"
#include "iosp.h"
#include "parse_file.h"
#include "patcode.h"
#include "compiler.h"
#include "lv_val.h"
#include "stringpool.h"
#include "stp_parms.h"
#include "error.h"
#include "eintr_wrappers.h"
#include "op.h"
#include "zroutines.h"
#include "mvalconv.h"
#include "gtmctype.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "alias.h"

GBLREF	symval		*curr_symval;
GBLREF	boolean_t	gtm_utf8_mode;
GBLREF	spdesc		stringpool;

LITREF mval	literal_null;

STATICFNDCL		CONDITION_HANDLER(fnzsrch_ch);
STATICFNDCL		CONDITION_HANDLER(dir_ch);
STATICFNDCL int		pop_top(lv_val *src, mval *res);

void		dir_srch(parse_blk *pfil);

error_def(ERR_ASSERT);
error_def(ERR_GTMASSERT);
error_def(ERR_GTMASSERT2);
error_def(ERR_GTMCHECK);
error_def(ERR_INVSTRLEN);
error_def(ERR_MEMORY);
error_def(ERR_STACKOFLOW);

int op_fnzsearch(mval *file, mint indx, mval *ret)
{
	struct stat	statbuf;
	int		stat_res;
	parse_blk	pblk;
	plength		*plen, pret;
	char		buf1[MAX_FBUFF + 1]; /* buffer to hold translated name */
	mval		sub;
	mstr		tn;
	lv_val		*ind_tmp;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	ESTABLISH_RET(fnzsrch_ch, -1);
	TREF(fnzsearch_nullsubs_sav) = TREF(lv_null_subs);
	TREF(lv_null_subs) = LVNULLSUBS_OK;	/* $ZSearch processing depends on this */
	MV_FORCE_STR(file);
	if (file->str.len > MAX_FBUFF)
		rts_error(VARLSTCNT(4) ERR_INVSTRLEN, 2, file->str.len, MAX_FBUFF);
	MV_FORCE_MVAL(((mval *)TADR(fnzsearch_sub_mval)), indx);
	TREF(fnzsearch_lv_vars) = op_srchindx(VARLSTCNT(2) TREF(zsearch_var), (mval *)TADR(fnzsearch_sub_mval));
	if (TREF(fnzsearch_lv_vars))
	{
		assert((TREF(fnzsearch_lv_vars))->v.mvtype & MV_STR);
		if ((file->str.len != (TREF(fnzsearch_lv_vars))->v.str.len)
			|| memcmp(file->str.addr, (TREF(fnzsearch_lv_vars))->v.str.addr, file->str.len))
		{
			op_kill(TREF(fnzsearch_lv_vars));
			TREF(fnzsearch_lv_vars) = NULL;
		}
	}
	if (TREF(fnzsearch_lv_vars))
	{
		for (;;)
		{
			pret.p.pint = pop_top(TREF(fnzsearch_lv_vars), ret);	/* get next element off the top */
			if (!ret->str.len)
				break;
			memcpy(buf1, ret->str.addr, ret->str.len);
			buf1[ret->str.len] = 0;
			STAT_FILE(buf1, &statbuf, stat_res);
			if (-1 == stat_res)
			{
				if (errno == ENOENT)
					continue;
				rts_error(VARLSTCNT(1) errno);
			}
			break;
		}
	} else
	{
		memset(&pblk, 0, SIZEOF(pblk));
		pblk.buffer = buf1;
		pblk.buff_size = MAX_FBUFF;
		if (!(parse_file(&file->str, &pblk) & 1))
		{
			ret->mvtype = MV_STR;
			ret->str.len = 0;
		} else
		{
			assert(!TREF(fnzsearch_lv_vars));
			buf1[pblk.b_esl] = 0;
			/* establish new search context */
			TREF(fnzsearch_lv_vars) = op_putindx(VARLSTCNT(2) TREF(zsearch_var), TADR(fnzsearch_sub_mval));
			(TREF(fnzsearch_lv_vars))->v = *file;	/* zsearch_var(indx)=original spec */
			if (!(pblk.fnb & F_WILD))
			{
				sub.mvtype = MV_STR;
				sub.str.len = pblk.b_esl;
				sub.str.addr =  buf1;
				s2pool(&sub.str);
				ind_tmp = op_putindx(VARLSTCNT(2) TREF(fnzsearch_lv_vars), &sub);
				ind_tmp->v.mvtype = MV_STR; ind_tmp->v.str.len = 0;
				plen = (plength *)&ind_tmp->v.m[1];
				plen->p.pblk.b_esl = pblk.b_esl;
				plen->p.pblk.b_dir = pblk.b_dir;
				plen->p.pblk.b_name = pblk.b_name;
				plen->p.pblk.b_ext = pblk.b_ext;
			} else
				dir_srch(&pblk);
			for (;;)
			{
				pret.p.pint = pop_top(TREF(fnzsearch_lv_vars), ret);	/* get next element off the top */
				if (!ret->str.len)
					break;
				memcpy(buf1, ret->str.addr, ret->str.len);
				buf1[ret->str.len] = 0;
				STAT_FILE(buf1, &statbuf, stat_res);
				if (-1 == stat_res)
				{
					if (errno == ENOENT)
						continue;
					rts_error(VARLSTCNT(1) errno);
				}
				break;
			}
		}
	}
	assert((0 == ret->str.len) || (pret.p.pblk.b_esl == ret->str.len));
	TREF(lv_null_subs) = TREF(fnzsearch_nullsubs_sav);
	REVERT;
	return pret.p.pint;
}

void dir_srch(parse_blk *pfil)
{
	struct stat	statbuf;
	int		stat_res;
	lv_val		*dir1, *dir2, *tmp;
	mstr		tn;
	short		p2_len;
	char		filb[MAX_FBUFF + 1], patb[SIZEOF(ptstr)], *c, *lastd, *top, *p2, *c1, ch;
	mval		pat_mval, sub, compare;
	boolean_t	wildname, seen_wd;
	struct dirent 	*dent;
	DIR		*dp;
	plength		*plen;
	int		closedir_res;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	op_kill(TREF(zsearch_dir1));
	op_kill(TREF(zsearch_dir2));
	if (!pfil->b_name)
		return;		/* nothing to search for */
	ESTABLISH(dir_ch);
	pat_mval.mvtype = MV_STR;
	pat_mval.str.addr = patb;	/* patb should be SIZEOF(ptstr.buff) but instead is SIZEOF(ptstr) since the C compiler
					 * complains about the former and the latter is just 4 bytes more */
	pat_mval.str.len = 0;
	sub.mvtype = MV_STR;
	sub.str.len = 0;
	compare.mvtype = MV_STR;
	compare.str.len = 0;
	wildname = (pfil->fnb & F_WILD_NAME) != 0;
	dir1 = TREF(zsearch_dir1);
	dir2 = TREF(zsearch_dir2);
	if (pfil->fnb & F_WILD_DIR)
	{
		seen_wd = FALSE;
		for (c = pfil->l_dir, lastd = c, top = c + pfil->b_dir; c < top;)
		{
			ch = *c++;
			if (ch == '/')	/* note the start of each directory segment */
			{
				if (seen_wd)
					break;
				lastd = c;
			}
			if (ch == '?' || ch == '*')
				seen_wd = TRUE;
		}
		assert(c <= top);
		sub.str.addr = pfil->l_dir;
		sub.str.len = INTCAST(lastd - sub.str.addr);
		tmp = op_putindx(VARLSTCNT(2) dir1, &sub);
		tmp->v.mvtype = MV_STR; tmp->v.str.len = 0;
		for (;;)
		{
			tn.addr = lastd;	/* wildcard segment */
			tn.len = INTCAST(c - lastd - 1);
			lastd = c;
			genpat(&tn, &pat_mval);
			seen_wd = FALSE;
			p2 = c - 1;
			for (; c < top;)
			{
				ch = *c++;
				if (ch == '/')	/* note the start of each directory segment */
				{
					if (seen_wd)
						break;
					lastd = c;
				}
				if (ch == '?' || ch == '*')
					seen_wd = TRUE;
			}
			p2_len = lastd - p2;	/* length of non-wild segment after wild section */
			for (;;)
			{
				pop_top(dir1, &sub);	/* get next item off the top */
				if (!sub.str.len)
					break;
				memcpy(filb, sub.str.addr, sub.str.len);
				filb[sub.str.len] = 0;
				sub.str.addr = filb;
				dp = OPENDIR(filb);
				if (!dp)
					continue;
				while (READDIR(dp, dent))
				{
					compare.str.addr = &dent->d_name[0];
					compare.str.len = STRLEN(&dent->d_name[0]);
					UNICODE_ONLY(
						if (gtm_utf8_mode)
							compare.mvtype &= ~MV_UTF_LEN;	/* to force "char_len" to be recomputed
											 * in do_pattern */
					)
					assert(compare.str.len);
					if (('.' == dent->d_name[0])
					    && ((1 == compare.str.len) || ((2 == compare.str.len) && ('.' == dent->d_name[1]))))
						continue;	/* don't want to read . and .. */
					if (compare.str.len + sub.str.len + p2_len > MAX_FBUFF)
						continue;
					if (do_pattern(&compare, &pat_mval))
					{	/* got a hit */
						ENSURE_STP_FREE_SPACE(compare.str.len + sub.str.len + p2_len + 1);
						/* concatenate directory and name */
						c1 = (char *)stringpool.free;
						tn = sub.str;
						s2pool(&tn);
						tn = compare.str;
						s2pool(&tn);
						tn.addr = p2;
						tn.len = p2_len;
						s2pool(&tn);
						*stringpool.free++ = 0;
						compare.str.addr = c1;
						compare.str.len += sub.str.len + p2_len;
						STAT_FILE(compare.str.addr, &statbuf, stat_res);
						if (-1 == stat_res)
							continue;
						if (!(statbuf.st_mode & S_IFDIR))
							continue;
						/* put in results tree */
						tmp = op_putindx(VARLSTCNT(2) dir2, &compare);
						tmp->v.mvtype = MV_STR;
						tmp->v.str.len = 0;
					}
				}
				CLOSEDIR(dp, closedir_res);
			}
			tmp = dir1; dir1 = dir2; dir2 = tmp;
			if (c >= top)
				break;
		}
	} else
	{
		sub.str.addr = pfil->l_dir;
		sub.str.len = pfil->b_dir;
		tmp = op_putindx(VARLSTCNT(2) dir1, &sub);
		tmp->v.mvtype = MV_STR; tmp->v.str.len = 0;
	}
	if (wildname)
	{
		tn.addr = pfil->l_name;
		tn.len = pfil->b_name + pfil->b_ext;
		genpat(&tn, &pat_mval);
	}
	for (;;)
	{
		pop_top(dir1, &sub);	/* get next item off the top */
		if (!sub.str.len)
			break;
		if (wildname)
		{
			memcpy(filb, sub.str.addr, sub.str.len);
			filb[sub.str.len] = 0;
			sub.str.addr = filb;
			dp = OPENDIR(filb);
			if (!dp)
				continue;
			while (READDIR(dp, dent))
			{
				compare.str.addr = &dent->d_name[0];
				compare.str.len = STRLEN(&dent->d_name[0]);
				UNICODE_ONLY(
					if (gtm_utf8_mode)
						compare.mvtype &= ~MV_UTF_LEN;/* force "char_len" to be recomputed in do_pattern */
				)
				if (('.' == dent->d_name[0])
				    && ((1 == compare.str.len) || ((2 == compare.str.len) && ('.' == dent->d_name[1]))))
				{
					continue;	/* don't want to read . and .. */
				}
				if (compare.str.len + sub.str.len > MAX_FBUFF)
					continue;
				if (do_pattern(&compare, &pat_mval))
				{	/* got a hit */
					ENSURE_STP_FREE_SPACE(compare.str.len + sub.str.len);
					/* concatenate directory and name */
					c = (char *)stringpool.free;
					tn = sub.str;
					s2pool(&tn);
					tn = compare.str;
					s2pool(&tn);
					compare.str.addr = c;
					compare.str.len += sub.str.len;
					/* put in results tree */
					tmp = op_putindx(VARLSTCNT(2) TREF(fnzsearch_lv_vars), &compare);
					tmp->v.mvtype = MV_STR;
					tmp->v.str.len = 0;
					plen = (plength *)&tmp->v.m[1];
					plen->p.pblk.b_esl = compare.str.len;
					plen->p.pblk.b_dir = sub.str.len;
					for (c = &compare.str.addr[sub.str.len], c1 = top = &compare.str.addr[compare.str.len];
					     c < top;)
					{
						if (*c++ != '.')
							break;
					}
					for (; c < top;)
					{
						if (*c++ == '.')
							c1 = c - 1;
					}
					plen->p.pblk.b_ext = top - c1;
					plen->p.pblk.b_name = plen->p.pblk.b_esl - plen->p.pblk.b_dir - plen->p.pblk.b_ext;
				}
			}
			CLOSEDIR(dp, closedir_res);
		} else
		{
			assert(pfil->fnb & F_WILD_DIR);
			compare.str.addr = pfil->l_name;
			compare.str.len = pfil->b_name + pfil->b_ext;
			if (compare.str.len + sub.str.len > MAX_FBUFF)
				continue;
			memcpy(filb, sub.str.addr, sub.str.len);
			filb[sub.str.len] = 0;
			sub.str.addr = filb;
			ENSURE_STP_FREE_SPACE(compare.str.len + sub.str.len);
			/* concatenate directory and name */
			c1 = (char *)stringpool.free;
			tn = sub.str;
			s2pool(&tn);
			tn = compare.str;
			s2pool(&tn);
			compare.str.addr = c1;
			compare.str.len += sub.str.len;
			/* put in results tree */
			tmp = op_putindx(VARLSTCNT(2) TREF(fnzsearch_lv_vars), &compare);
			tmp->v.mvtype = MV_STR; tmp->v.str.len = 0;
			plen = (plength *)&tmp->v.m[1];
			plen->p.pblk.b_esl = compare.str.len;
			plen->p.pblk.b_dir = sub.str.len;
			plen->p.pblk.b_name = pfil->b_name;
			plen->p.pblk.b_ext = pfil->b_ext;
		}
	}
	op_kill(TREF(zsearch_dir1));
	op_kill(TREF(zsearch_dir2));
	REVERT;
}

STATICFNDEF CONDITION_HANDLER(fnzsrch_ch)
{
	int	dummy1, dummy2;

	START_CH;
	TREF(lv_null_subs) = TREF(fnzsearch_nullsubs_sav);
	NEXTCH;
}

STATICFNDEF CONDITION_HANDLER(dir_ch)
{
	int	dummy1, dummy2;

	START_CH;
	if (DUMP)
	{
		NEXTCH;
	}
	op_kill(TREF(zsearch_dir1));
	op_kill(TREF(zsearch_dir2));
	op_kill(TREF(fnzsearch_lv_vars));
	TREF(fnzsearch_lv_vars) = op_putindx(VARLSTCNT(2) TREF(zsearch_var), TADR(fnzsearch_sub_mval));
	(TREF(fnzsearch_lv_vars))->v.mvtype = MV_STR;
	(TREF(fnzsearch_lv_vars))->v.str.len = 0;
	UNWIND(dummy1, dummy2);
}

void	zsrch_clr(int indx)
{
	lv_val	*tmp;
	mval	x;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_MVAL(&x, indx);
	op_kill(TREF(zsearch_dir1));
	op_kill(TREF(zsearch_dir2));
	tmp = op_srchindx(VARLSTCNT(2) TREF(zsearch_var), &x);
	op_kill(tmp);
}

/* pop_top() - routine scans the specified local variable (in this case, special locals used by $ZSEARCH), taking the top
 * subscript out of the array, and putting it in the result mval.  Subscripts longer than the parse_file() maximum are ignored.
 */
STATICFNDEF int pop_top(lv_val *src, mval *res)
{
	lv_val	*tmp;
	plength	pret;

	for (;;)
	{	/* get next element off the top */
		op_fnorder(src, (mval *)&literal_null, res);
		if (!res->str.len)
		{
			pret.p.pint = 0;
			op_kill(src);
			break;
		}
		tmp = op_getindx(VARLSTCNT(2) src, res);
		pret.p.pint = tmp->v.m[1];
		op_kill(tmp);	/* remove this element from tree */
		if (res->str.len > MAX_FBUFF)
			continue;
		break;
	}
	return pret.p.pint;
}
