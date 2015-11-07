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
#include "stringpool.h"
#include <descrip.h>
#include "desc2mval.h"
#include "mvalconv.h"

GBLREF spdesc stringpool;

error_def(ERR_ERRCALL);
error_def(ERR_MAXSTRLEN);
error_def(ERR_UNSDCLASS);
error_def(ERR_UNSDDTYPE);

void desc2mval (struct dsc$descriptor *src, mval *v)
{
	if ($is_desc64(src))
		desc2mval_64((struct dsc64$descriptor *)src, v);
	else
		desc2mval_32(src, v);
}

void desc2mval_32(struct dsc$descriptor *src, mval *v)
{
	int4		status;
	struct dsc$descriptor	dst;
	double		dstnm;

	switch(src->dsc$b_class)
	{
		case DSC$K_CLASS_S:	/* scalar or string descriptor */
		case DSC$K_CLASS_D:	/* dynamic descriptor is same as _S for input */
			v->mvtype = MV_STR;
			switch(src->dsc$b_dtype)
			{
				case DSC$K_DTYPE_G:
					double2s(src->dsc$a_pointer, v);
					break;
				case DSC$K_DTYPE_B:
					MV_FORCE_MVAL(v, *(char *)src->dsc$a_pointer);
					break;
				case DSC$K_DTYPE_BU:
					MV_FORCE_MVAL(v, *(unsigned char *)src->dsc$a_pointer);
					break;
				case DSC$K_DTYPE_W:
					MV_FORCE_MVAL(v, *(short *)src->dsc$a_pointer);
					break;
				case DSC$K_DTYPE_WU:
					MV_FORCE_MVAL(v, *(unsigned short *)src->dsc$a_pointer);
					break;
				case DSC$K_DTYPE_L:
					MV_FORCE_MVAL(v, *(int4 *)src->dsc$a_pointer);
					break;
				case DSC$K_DTYPE_LU:
				case DSC$K_DTYPE_Q:
				case DSC$K_DTYPE_QU:
				case DSC$K_DTYPE_D:
				case DSC$K_DTYPE_F:
				case DSC$K_DTYPE_H:
					dst.dsc$w_length	= SIZEOF(double);
					dst.dsc$b_dtype		= DSC$K_DTYPE_G;
					dst.dsc$b_class		= DSC$K_CLASS_S;
					dst.dsc$a_pointer	= &dstnm;
					status = lib$cvt_dx_dx(src, &dst);
					if (!(status & 1))
						rts_error(VARLSTCNT(1) status);
					double2s(&dstnm, v);
					break;
				case DSC$K_DTYPE_T:
					ENSURE_STP_FREE_SPACE(src->dsc$w_length);
					assert(stringpool.free >= stringpool.base);
					v->str.addr = stringpool.free;
					stringpool.free += v->str.len = src->dsc$w_length;
					assert(stringpool.free <= stringpool.top);
					memcpy(v->str.addr, src->dsc$a_pointer, v->str.len);
					break;
				default:
					rts_error(VARLSTCNT(1) ERR_UNSDDTYPE);
			}
			break;
		default:
			rts_error(VARLSTCNT(7) ERR_UNSDCLASS, 5, ERR_ERRCALL, 3, CALLFROM);
	}
}

void desc2mval_64 (struct dsc64$descriptor *src, mval *v)
{
	int4		status;
	struct dsc64$descriptor	dst;
	double		dstnm;

	switch(src->dsc64$b_class)
	{
		case DSC64$K_CLASS_S:	/* scalar or string descriptor */
		case DSC64$K_CLASS_D:	/* dynamic descriptor is same as _S for input */
			v->mvtype = MV_STR;
			switch(src->dsc64$b_dtype)
			{
				case DSC64$K_DTYPE_G:
					double2s((double *)src->dsc64$pq_pointer, v);
					break;
				case DSC64$K_DTYPE_B:
					MV_FORCE_MVAL(v, *(char *)src->dsc64$pq_pointer);
					break;
				case DSC64$K_DTYPE_BU:
					MV_FORCE_MVAL(v, *(unsigned char *)src->dsc64$pq_pointer);
					break;
				case DSC64$K_DTYPE_W:
					MV_FORCE_MVAL(v, *(short *)src->dsc64$pq_pointer);
					break;
				case DSC64$K_DTYPE_WU:
					MV_FORCE_MVAL(v, *(unsigned short *)src->dsc64$pq_pointer);
					break;
				case DSC64$K_DTYPE_L:
					MV_FORCE_MVAL(v, *(int4 *)src->dsc64$pq_pointer);
					break;
				case DSC64$K_DTYPE_LU:
				case DSC64$K_DTYPE_Q:
				case DSC64$K_DTYPE_QU:
				case DSC64$K_DTYPE_FS:
				case DSC64$K_DTYPE_F:
				case DSC64$K_DTYPE_D:
				case DSC64$K_DTYPE_H:
					dst.dsc64$w_mbo = 1;
					dst.dsc64$l_mbmo = -1;
					dst.dsc64$q_length = SIZEOF(double);
					dst.dsc64$b_dtype = DSC64$K_DTYPE_G;
					dst.dsc64$b_class = DSC64$K_CLASS_S;
					dst.dsc64$pq_pointer = (char *)&dstnm;
					status = lib$cvt_dx_dx(src, &dst);
					if (!(status & 1))
						rts_error(VARLSTCNT(1) status);
					double2s(&dstnm, v);
					break;
				case DSC64$K_DTYPE_T:
					if(MAX_STRLEN < src->dsc64$q_length)
						rts_error(VARLSTCNT(1) ERR_MAXSTRLEN);
					ENSURE_STP_FREE_SPACE(src->dsc64$q_length);
					assert(stringpool.free >= stringpool.base);
					v->str.addr = stringpool.free;
					stringpool.free += v->str.len = src->dsc64$q_length;
					assert(stringpool.free <= stringpool.top);
					memcpy(v->str.addr, src->dsc64$pq_pointer, v->str.len);
					break;
				default:
					rts_error(VARLSTCNT(1) ERR_UNSDDTYPE);
			}
			break;
		default:
			rts_error(VARLSTCNT(7) ERR_UNSDCLASS, 5, ERR_ERRCALL, 3, CALLFROM);
	}
}
