/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
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

#include <errno.h>
#include "zroutines.h"
#include "eintr_wrappers.h"

GBLREF	zro_ent		*zro_root;
GBLREF  int		errno;

void zro_search (mstr *objstr, zro_ent **objdir, mstr *srcstr, zro_ent **srcdir)
/*
mstr		*objstr;	if zero, do not search for object, else pointer to object file text string
zro_ent		**objdir;	zero if objstr is zero, otherwise, return pointer to associated object directory
					objdir is zero if object directory is not found
mstr		*srcstr;	like objstr, except for associated source program
zro_ent		**srcdir;	like objdir, except for associated source program directory
*/
{
	uint4	status;
	zro_ent		*op, *sp, *op_result, *sp_result;
	char		objfn[255], srcfn[255], *obp, *sbp;
	int		objcnt, srccnt;
	struct  stat	outbuf;
	int		stat_res;
	error_def	(ERR_ZFILENMTOOLONG);

	if (!zro_root)
		zro_init ();
	assert(objstr || srcstr);	/* must search for object or source or both */
	op_result = sp_result = 0;
	if (objstr)
	{	assert(objdir);		/* if object text, then must have pointer for result */
	}
	if (srcstr)
	{	assert(srcdir);		/* if source text, then must have pointer for result */
	}
	assert (zro_root->type == ZRO_TYPE_COUNT);
	objcnt = zro_root->count;
	assert (objcnt);
	for (op = zro_root + 1; !op_result && !sp_result && objcnt-- > 0; )
	{
		assert (op->type == ZRO_TYPE_OBJECT);
		if (objstr)
		{
			if (op->str.len + objstr->len + 2 > sizeof objfn)
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
					rts_error(VARLSTCNT(1) errno);
			}
			else
					op_result = op;
		}
		if (srcstr)
		{
			sp = op + 1;
			assert (sp->type == ZRO_TYPE_COUNT);
			srccnt = sp++->count;
			for ( ; !sp_result && srccnt-- > 0; sp++)
			{
				assert (sp->type == ZRO_TYPE_SOURCE);
				if (sp->str.len + srcstr->len + 2 > sizeof srcfn)
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
						rts_error(VARLSTCNT(1) errno);
				}
				else
				{	sp_result = sp;
					op_result = op;
				}
			}
			op = sp;
		}
		else
		{
			op++;
			assert (op->type == ZRO_TYPE_COUNT);
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
