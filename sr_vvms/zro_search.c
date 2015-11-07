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
#include <rms.h>

#include "zroutines.h"
#include "zl_olb.h"

error_def	(ERR_ZFILENMTOOLONG);

/* if NULL == objstr, do not search for object, else pointer to object file text string */
/* objdir is NULL if objstr is NULL, otherwise, return pointer to associated object directory
					*objdir is NULL if object directory is not found */
/* srcstr is like objstr, except for associated source program */
/* srcdir is like objdir, except for associated source program directory*/
void zro_search(mstr *objstr, zro_ent **objdir, mstr *srcstr, zro_ent **srcdir)
{
	unsigned	obj_status, src_status;
	uint4		librindx, status;
	zro_ent		*op, *sp, *op_result, *sp_result;
	struct FAB	objfab, srcfab;
	struct NAM	objnam, srcnam;
	unsigned char	objfn[255], srcfn[255];
	unsigned char	objes[255], srces[255];
	int		objcnt, srccnt, namidx;
	mstr		obj_string;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (!TREF(zro_root))
		zro_init();
	assert(objstr || srcstr);	/* must search for object or source or both */
	op_result = sp_result = NULL;
	if (objstr)
	{
		assert(objdir);		/* if object text, then must have pointer for result */
		objfab = cc$rms_fab;
		objnam = cc$rms_nam;
		objfab.fab$l_nam = &objnam;
	}
	if (srcstr)
	{
		assert(srcdir);		/* if source text, then must have pointer for result */
		srcfab = cc$rms_fab;
		srcnam = cc$rms_nam;
		srcfab.fab$l_nam = &srcnam;
	}
	assert((TREF(zro_root))->type == ZRO_TYPE_COUNT);
	objcnt = (TREF(zro_root))->count;
	assert(objcnt);
	for (op = TREF(zro_root) + 1; !op_result && !sp_result && objcnt-- > 0;)
	{
		assert(op->type == ZRO_TYPE_OBJECT || op->type == ZRO_TYPE_OBJLIB);
		if (objstr)
		{
			if (op->type == ZRO_TYPE_OBJECT)
			{
				if (op->str.len > SIZEOF(objfn))
					rts_error(VARLSTCNT(4) ERR_ZFILENMTOOLONG, 2, op->str.len, op->str.addr);
				memcpy(objfn, op->str.addr, op->str.len);
				namidx = op->str.len;
				if (':' != objfn[namidx - 1] && ']' != objfn[namidx - 1]
				       && '>' != objfn[namidx - 1])
				    objfn[namidx++] = ':';
				if (namidx + objstr->len > SIZEOF(objfn))
					rts_error(VARLSTCNT(4) ERR_ZFILENMTOOLONG, 2, op->str.len, op->str.addr);
				memcpy(&objfn[namidx], objstr->addr, objstr->len);
				objfab.fab$l_fna = objfn;
				objfab.fab$b_fns = namidx + objstr->len;
				objnam.nam$l_esa = objes;
				objnam.nam$b_ess = SIZEOF(objes);
				obj_status = sys$parse(&objfab);
				if (!(obj_status & 1))
					rts_error(VARLSTCNT(2) obj_status, objfab.fab$l_stv);
				objnam.nam$l_wcc = 0;
				obj_status = sys$search(&objfab);
				switch (obj_status)
				{
					case RMS$_NORMAL:
						op_result = op;
						break;
					case RMS$_FNF:
					case RMS$_NMF:
						break;
					default:
						rts_error(VARLSTCNT(2) obj_status, objfab.fab$l_stv);
				}
			} else
			{
				obj_status = zl_olb(&op->str, objstr, &librindx);
				status = lbr$close(&librindx);
				if (!(status & 1))
					rts_error(VARLSTCNT(1) status);
				if (1 & obj_status)
					op_result = op;
			}
		}
		if (srcstr)
		{
			sp = op + 1;
			assert(sp->type == ZRO_TYPE_COUNT);
			srccnt = sp++->count;
			for (;  !sp_result && srccnt-- > 0;  sp++)
			{
				assert(sp->type == ZRO_TYPE_SOURCE);
				if (sp->str.len > SIZEOF(srcfn))
					rts_error(VARLSTCNT(4) ERR_ZFILENMTOOLONG, 2, sp->str.len, sp->str.addr);
      				memcpy(srcfn, sp->str.addr, sp->str.len);
				namidx = sp->str.len;
				if (':' != srcfn[namidx - 1] && ']' != srcfn[namidx - 1]
				       && '>' != srcfn[namidx - 1])
				    srcfn[namidx++] = ':';
				if (namidx + srcstr->len > SIZEOF(srcfn))
					rts_error(VARLSTCNT(4) ERR_ZFILENMTOOLONG, 2, sp->str.len, sp->str.addr);
      				memcpy(&srcfn[namidx], srcstr->addr, srcstr->len);
				srcfab.fab$l_fna = srcfn;
				srcfab.fab$b_fns = namidx + srcstr->len;
				srcnam.nam$l_esa = srces;
				srcnam.nam$b_ess = SIZEOF(srces);
				src_status = sys$parse(&srcfab);
				if (!(src_status & 1))
					rts_error(VARLSTCNT(2) src_status, srcfab.fab$l_stv);
				srcnam.nam$l_wcc = 0;
				src_status = sys$search(&srcfab);
				switch (src_status)
				{
				case RMS$_NORMAL:
					sp_result = sp;
					op_result = op;
					break;
				case RMS$_FNF:
				case RMS$_NMF:
					break;
				default:
					rts_error(VARLSTCNT(2) src_status, srcfab.fab$l_stv);
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
