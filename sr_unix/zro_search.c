/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
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
#include "gtm_limits.h"

#include <errno.h>
#include "zroutines.h"
#include "eintr_wrappers.h"
#include "error.h"
#include "fgncal.h"

GBLREF	zro_ent		*zro_root;

void zro_search (mstr *objstr, zro_ent **objdir, mstr *srcstr, zro_ent **srcdir, boolean_t skip)
/*
mstr		*objstr;	if NULL, do not search for object, else pointer to object file text string
zro_ent		**objdir;	NULL if objstr is NULL, otherwise, return pointer to associated object directory
					objdir is NULL if object directory is not found
mstr		*srcstr;	like objstr, except for associated source program
zro_ent		**srcdir;	like objdir, except for associated source program directory
boolean_t	skip;		if TRUE, skip over shared libraries. If FALSE, probe shared libraries.
*/
{
	uint4	status;
	zro_ent		*op, *sp, *op_result, *sp_result;
	char		objfn[PATH_MAX], srcfn[PATH_MAX], *obp, *sbp;
	int		objcnt, srccnt;
	struct  stat	outbuf;
	int		stat_res;
	mstr		rtnname;
	error_def	(ERR_ZFILENMTOOLONG);
	error_def	(ERR_SYSCALL);

	if (!zro_root)
		zro_init ();
	assert(objstr || srcstr);	/* must search for object or source or both */
	assert(!objstr || objdir);	/* if object text, then must have pointer for result */
	assert(!srcstr || srcdir);	/* if source text, then must have pointer for result */
	assert(zro_root->type == ZRO_TYPE_COUNT);
	op_result = sp_result = NULL;
	objcnt = zro_root->count;
	assert(objcnt);
	for (op = zro_root + 1; !op_result && !sp_result && objcnt-- > 0; )
	{
		assert(op->type == ZRO_TYPE_OBJECT || op->type == ZRO_TYPE_OBJLIB);
		if (objstr)
		{
			if (op->type == ZRO_TYPE_OBJLIB)
			{
				if (!skip)
				{
					assert(op->shrlib);
					rtnname.len = objstr->len - STR_LIT_LEN(DOTOBJ);
					memcpy(objfn, objstr->addr, rtnname.len);
					objfn[rtnname.len] = 0;
					rtnname.addr = objfn;
					if ((op->shrsym = fgn_getrtn(op->shrlib, &rtnname, SUCCESS)) != NULL)
						op_result = op;
				}
				op++;
				continue;
			}
			if (op->str.len + objstr->len + 2 > sizeof(objfn))
				rts_error(VARLSTCNT(4) ERR_ZFILENMTOOLONG, 2, op->str.len, op->str.addr);
			obp = &objfn[0];
			if (op->str.len)
			{
				memcpy (obp, op->str.addr, op->str.len);
				obp += op->str.len;
				*obp++ = '/';
			}
			memcpy (obp, objstr->addr, objstr->len);
			obp += objstr->len;
			*obp++ = 0;
			STAT_FILE(objfn, &outbuf, stat_res);
			if (-1 == stat_res)
			{
				if (errno != ENOENT)
					rts_error(VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("stat"), CALLFROM, errno);
			} else
				op_result = op;
		}
		if (srcstr)
		{
			sp = op + 1;
			if (op->type == ZRO_TYPE_OBJLIB)
			{
				op = sp;
				continue;
			}
			assert(sp->type == ZRO_TYPE_COUNT);
			srccnt = (sp++)->count;
			for ( ; !sp_result && srccnt-- > 0; sp++)
			{
				assert(sp->type == ZRO_TYPE_SOURCE);
				if (sp->str.len + srcstr->len + 2 > sizeof(srcfn)) /* extra 2 for '/' & null */
					rts_error(VARLSTCNT(4) ERR_ZFILENMTOOLONG, 2, sp->str.len, sp->str.addr);
				sbp = &srcfn[0];
				if (sp->str.len)
				{
					memcpy (sbp, sp->str.addr, sp->str.len);
					sbp += sp->str.len;
					*sbp++ = '/';
				}
				memcpy (sbp, srcstr->addr, srcstr->len);
				sbp += srcstr->len;
				*sbp++ = 0;
				STAT_FILE(srcfn, &outbuf, stat_res);
				if (-1 == stat_res)
				{
					if (errno != ENOENT)
						rts_error(VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("stat"), CALLFROM, errno);
				} else
				{	sp_result = sp;
					op_result = op;
				}
			}
			op = sp;
		} else
		{
			op++;
			assert(op->type == ZRO_TYPE_COUNT);
			op += op->count;
			op++;
		}
	}
	if (objdir)
		*objdir = op_result;
	if (srcdir)
		*srcdir = sp_result;
	return;
}
