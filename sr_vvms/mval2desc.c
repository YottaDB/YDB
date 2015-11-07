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
#include <descrip.h>
#include "mvalconv.h"
#include "mval2desc.h"

error_def(ERR_ERRCALL);
error_def(ERR_NUMOFLOW);
error_def(ERR_SYSCALL);
error_def(ERR_UNSDCLASS);
error_def(ERR_UNSDDTYPE);

void mval2desc(mval *v, struct dsc$descriptor *d)
{
	if ($is_desc64(d))
		mval2desc_64(v, (struct dsc64$descriptor *)d);
	else
		mval2desc_32(v, d);
}

void mval2desc_32(mval *v, struct dsc$descriptor *d)
{
	int4		status;
	int4		lx;
	struct dsc$descriptor	src;
	double		srcnm;

	switch(d->dsc$b_class)
	{
		case DSC$K_CLASS_D:	/* dynamic string descriptor */
			switch(d->dsc$b_dtype)
			{
				case DSC$K_DTYPE_T:
					MV_FORCE_STR(v);
					if (v->str.len != d->dsc$w_length)
					{	/* re-allocate descriptor if length doesn't match */
						if (d->dsc$a_pointer) /* free only if already allocated */
						{
							status = lib$sfree1_dd(d);
							if (0 == (status & 1))
								rts_error(VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("LIB$SFREE1_DD"),
									  CALLFROM, status);
						}
						status = lib$sget1_dd(&v->str.len, d);
						if (0 == (status & 1))
							rts_error(VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("LIB$SGET1_DD"),
								  CALLFROM, status);
					}
					memcpy(d->dsc$a_pointer, v->str.addr, v->str.len);
					break;
				default:
					rts_error(VARLSTCNT(1) ERR_UNSDDTYPE);
			}
			break;
		case DSC$K_CLASS_S:	/* scalar or string descriptor */
			switch(d->dsc$b_dtype)
			{
				case DSC$K_DTYPE_T:
					MV_FORCE_STR(v);
					lx = v->str.len;
					if (lx > d->dsc$w_length)
						lx = d->dsc$w_length;
					if (lx)
						memcpy(d->dsc$a_pointer, v->str.addr, lx);
					if (d->dsc$w_length > lx)
						memset(d->dsc$a_pointer + lx, 0, d->dsc$w_length - lx);
					break;
				case DSC$K_DTYPE_G:
					MV_FORCE_NUM(v);
					*(double *)d->dsc$a_pointer = mval2double(v);
					break;
				case DSC$K_DTYPE_B:
					lx = MV_FORCE_INT(v);
					if (lx > 127 || lx < -127)
						rts_error(VARLSTCNT(1) ERR_NUMOFLOW);
					*(char *)d->dsc$a_pointer = lx;
					break;
				case DSC$K_DTYPE_BU:
					lx = MV_FORCE_INT(v);
					if (lx > 255)
						rts_error(VARLSTCNT(1) ERR_NUMOFLOW);
					*(unsigned char *)d->dsc$a_pointer = lx;
					break;
				case DSC$K_DTYPE_W:
					lx = MV_FORCE_INT(v);
					if (lx > 32767 || lx < -32767)
						rts_error(VARLSTCNT(1) ERR_NUMOFLOW);
					*(short *)d->dsc$a_pointer = lx;
					break;
				case DSC$K_DTYPE_WU:
					lx = MV_FORCE_INT(v);
					if (lx > 65535)
						rts_error(VARLSTCNT(1) ERR_NUMOFLOW);
					*(unsigned short *)d->dsc$a_pointer = lx;
					break;
				case DSC$K_DTYPE_LU:
					/* This case has been separated from the group immediately below
					   to get around a bug in lib$cvt_dx_dx introduced in OpenVMS AXP V6.1
					   that returns LIB$_INTOVF for any LU value greater than 2147483647. */
					MV_FORCE_NUM(v);
					srcnm = mval2double(v);
					if (srcnm > 4294967295.0)
						rts_error(VARLSTCNT(1) ERR_NUMOFLOW);
					*(uint4 *)d->dsc$a_pointer = srcnm;
					break;
				case DSC$K_DTYPE_L:
				case DSC$K_DTYPE_Q:
				case DSC$K_DTYPE_QU:
				case DSC$K_DTYPE_D:
				case DSC$K_DTYPE_F:
				case DSC$K_DTYPE_H:
					MV_FORCE_NUM(v);
					srcnm = mval2double(v);
					src.dsc$w_length	= 0;
					src.dsc$b_dtype		= DSC$K_DTYPE_G;
					src.dsc$b_class		= DSC$K_CLASS_S;
					src.dsc$a_pointer	= &srcnm;
					status = lib$cvt_dx_dx(&src, d);
					if (0 == (status & 1))
						rts_error(VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("LIB$CVT_DX_DX"),
							  CALLFROM, status);
					break;
				default:
					rts_error(VARLSTCNT(1) ERR_UNSDDTYPE);
			}
			break;
		default:
			rts_error(VARLSTCNT(7) ERR_UNSDCLASS, 5, ERR_ERRCALL, 3, CALLFROM);
	}
}

void mval2desc_64(mval *v, struct dsc64$descriptor *d)
{
	int4		status;
	int4		lx;
	struct dsc64$descriptor	src;
	double		srcnm;

	switch(d->dsc64$b_class)
	{

		case DSC64$K_CLASS_D:	/* dynamic string descriptor */
			switch(d->dsc64$b_dtype)
			{
				case DSC64$K_DTYPE_T:
					MV_FORCE_STR(v);
					if (v->str.len != d->dsc64$q_length)
					{	/* re-allocate descriptor if length doesn't match */
						if (d->dsc64$pq_pointer) /* free only if already allocated */
						{
							status = lib$sfree1_dd(d);
							if (0 == (status & 1))
								rts_error(VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("LIB$SFREE1_DD"),
									  CALLFROM, status);
						}
						d->dsc64$q_length = v->str.len;
						status = lib$sget1_dd_64(&d->dsc64$q_length, d);
						if (0 == (status & 1))
							rts_error(VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("LIB$SGET1_DD_64"),
								  CALLFROM, status);
					}
					memcpy(d->dsc64$pq_pointer, v->str.addr, v->str.len);
					break;
				default:
					rts_error(VARLSTCNT(1) ERR_UNSDDTYPE);
			}
			break;
		case DSC64$K_CLASS_S:	/* scalar or string descriptor */
			switch(d->dsc64$b_dtype)
			{
				case DSC64$K_DTYPE_T:
					MV_FORCE_STR(v);
					lx = v->str.len;
					if (lx > d->dsc64$q_length)
						lx = d->dsc64$q_length;
					if (lx)
						memcpy(d->dsc64$pq_pointer, v->str.addr, lx);
					if (d->dsc64$q_length > lx)
						memset(d->dsc64$pq_pointer + lx, 0, d->dsc64$q_length - lx);
					break;
				case DSC64$K_DTYPE_G:
					MV_FORCE_NUM(v);
					*(double *)d->dsc64$pq_pointer = mval2double(v);
					break;
				case DSC64$K_DTYPE_B:
					lx = MV_FORCE_INT(v);
					if (lx > 127 || lx < -127)
						rts_error(VARLSTCNT(1) ERR_NUMOFLOW);
					*(char *)d->dsc64$pq_pointer = lx;
					break;
				case DSC64$K_DTYPE_BU:
					lx = MV_FORCE_INT(v);
					if (lx > 255)
						rts_error(VARLSTCNT(1) ERR_NUMOFLOW);
					*(unsigned char *)d->dsc64$pq_pointer = lx;
					break;
				case DSC64$K_DTYPE_W:
					lx = MV_FORCE_INT(v);
					if (lx > 32767 || lx < -32767)
						rts_error(VARLSTCNT(1) ERR_NUMOFLOW);
					*(short *)d->dsc64$pq_pointer = lx;
					break;
				case DSC64$K_DTYPE_WU:
					lx = MV_FORCE_INT(v);
					if (lx > 65535)
						rts_error(VARLSTCNT(1) ERR_NUMOFLOW);
					*(unsigned short *)d->dsc64$pq_pointer = lx;
					break;
				case DSC64$K_DTYPE_LU:
					/* This case has been separated from the group immediately below
					   to get around a bug in lib$cvt_dx_dx introduced in OpenVMS AXP V6.1
					   that returns LIB$_INTOVF for any LU value greater than 2147483647. */
					MV_FORCE_NUM(v);
					srcnm = mval2double(v);
					if (srcnm > 4294967295.0)
						rts_error(VARLSTCNT(1) ERR_NUMOFLOW);
					*(uint4 *)d->dsc64$pq_pointer = srcnm;
					break;
				case DSC64$K_DTYPE_L:
				case DSC64$K_DTYPE_Q:
				case DSC64$K_DTYPE_QU:
				case DSC64$K_DTYPE_F:
				case DSC64$K_DTYPE_D:
				case DSC64$K_DTYPE_H:
					MV_FORCE_NUM(v);
					srcnm = mval2double(v);
					src.dsc64$w_mbo = 1;
					src.dsc64$l_mbmo = -1;
					src.dsc64$q_length = 0;
					src.dsc64$b_dtype = DSC64$K_DTYPE_G;
					src.dsc64$b_class = DSC64$K_CLASS_S;
					src.dsc64$pq_pointer	= (char *)&srcnm;
					status = lib$cvt_dx_dx(&src, d);
					if (0 == (status & 1))
						rts_error(VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("LIB$CVT_DX_DX"), CALLFROM,
							  status);
					break;
				default:
					rts_error(VARLSTCNT(1) ERR_UNSDDTYPE);
			}
			break;
		default:
			rts_error(VARLSTCNT(7) ERR_UNSDCLASS, 5, ERR_ERRCALL, 3, CALLFROM);
	}
}
