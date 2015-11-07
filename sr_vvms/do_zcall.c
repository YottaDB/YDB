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
#include <ssdef.h>
#include <descrip.h>
#include <inttypes.h>
#include <libdef.h>
#include "stringpool.h"
#include "zcall.h"
#include "zcdef.h"
#include "io.h"
#include "mval2desc.h"
#include "desc2mval.h"
#include "setterm.h"
#include "gtmdbglvl.h"		/* for VERIFY_STORAGE_CHAINS macro */
#include "gtm_malloc.h"		/* for VERIFY_STORAGE_CHAINS macro */

typedef	struct lclarg_type
{
	unsigned char		skip;
	unsigned char		initted;
	unsigned char		filler[2];
	unsigned char		*zctab;
	struct dsc64$descriptor	dsc;
	unsigned char		data[16];
} lclarg;

#define	VAL_NONE	0
#define	VAL_INPUTMVAL	1
#define	VAL_ZCTABVAL	2
#define	N_DSIZES	32
/*	memset(dsizes, 0, N_DSIZES * SIZEOF(dsizes[0]));
	dsizes[ZC$DTYPE_STRING]		= -1;
	dsizes[ZC$DTYPE_BYTE]		= 1;
	dsizes[ZC$DTYPE_WORD]		= 2;
	dsizes[ZC$DTYPE_LONG]		= 4;
	dsizes[ZC$DTYPE_QUAD]		= 8;
	dsizes[ZC$DTYPE_FLOATING]	= 4;
	dsizes[ZC$DTYPE_DOUBLE]		= 8;
	dsizes[ZC$DTYPE_G_FLOATING]	= 8;
	dsizes[ZC$DTYPE_H_FLOATING]	= 16;
*/
#define INIT_64BITDESC							\
{									\
				is_a_desc64 = TRUE;			\
				dsc64 = &lclp->dsc;			\
				dsc64->dsc64$w_mbo = 1;			\
				dsc64->dsc64$l_mbmo = -1;		\
				type = &dsc64->dsc64$b_dtype;		\
				class = &dsc64->dsc64$b_class;		\
}

#define INIT_32BITDESC								\
{										\
				is_a_desc64 = FALSE;				\
				dsc = (struct dsc$descriptor *)&lclp->dsc;	\
				type = &dsc->dsc$b_dtype;			\
				class = &dsc->dsc$b_class;			\
}

static readonly short	dsizes[N_DSIZES] = {
	0, 0, 1, 2, 4, 0, 1, 2, 4, 8, 4, 8,
	0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 8, 16, 0, 0, 0
};

GBLREF spdesc	stringpool;
GBLREF io_pair	io_std_device;

error_def(ERR_MAXSTRLEN);
error_def(ERR_VMSMEMORY2);
error_def(ERR_ZCALLTABLE);
error_def(ERR_ZCCONMSMTCH);
error_def(ERR_ZCCONVERT);
error_def(ERR_ZCINPUTREQ);
error_def(ERR_ZCOPT0);
error_def(ERR_ZCPOSOVR);
error_def(ERR_ZCUNKMECH);
error_def(ERR_ZCUNKQUAL);
error_def(ERR_ZCUNKTYPE);
error_def(ERR_ZCWRONGDESC);

void do_zcall(mval		*dst,
	      int		mask,
	      mval		**mvallist, mval **mvallistend,
	      zctabrtn		*zcrtn,
	      lclarg		*lcllist, lclarg *lcllistend)
{
	zctabret		*zcret;
	zctabinput		*firstin;
	zctaboutput		*firstout, *lastout;
	int4			status, alloclen;
	zctabinput		*inp;
	zctaboutput		*outp;
	mval			**mvpp;
	lclarg			*lclp;
	struct dsc64$descriptor	*dsc64;
	struct dsc$descriptor	*dsc, *valdsc;
	unsigned char 		*type;
	unsigned char		*class;
	boolean_t		is_a_desc64;
	unsigned char		use_value;
	union
	{
		double		f0;
		int		r0;
	} save_ret;
	uint4			dstlen, indx, byref;
	struct dsc$descriptor	tmpdsc;
	mval			tmpmval;
	uint64_t		val_qw;
	unsigned short		val_us;

	zcret = (zctabret *)((char *)zcrtn + SIZEOF(zctabrtn) - 1 + zcrtn->callnamelen);
	firstin = (zctabinput *)ROUND_UP((int) zcret + SIZEOF(zctabret), SIZEOF(int4));
	firstout = firstin + zcrtn->n_inputs;
	lastout = firstout + zcrtn->n_outputs;
	for (lclp = lcllist; lclp < lcllistend; lclp++)
		lclp->initted = FALSE;
	is_a_desc64 = FALSE;
	for (inp = firstin, mvpp = mvallist; inp < (zctabinput *)firstout; inp++)
	{
		switch (inp->type)	/* guard type */
		{
			case ZC$DTYPE_STRING:
			case ZC$DTYPE_BYTE:
			case ZC$DTYPE_BYTEU:
			case ZC$DTYPE_WORD:
			case ZC$DTYPE_WORDU:
			case ZC$DTYPE_LONG:
			case ZC$DTYPE_LONGU:
			case ZC$DTYPE_QUAD:
			case ZC$DTYPE_FLOATING:
			case ZC$DTYPE_DOUBLE:
			case ZC$DTYPE_H_FLOATING:
				break;
			default:
				rts_error(VARLSTCNT(1) ERR_ZCUNKTYPE);
		}
		switch (inp->mechanism)	/* guard mechanism */
		{
			case ZC$MECH_VALUE:
			case ZC$MECH_REFERENCE:
			case ZC$MECH_DESCRIPTOR:
			case ZC$MECH_DESCRIPTOR64:
				break;
			default:
				rts_error(VARLSTCNT(1) ERR_ZCUNKMECH);
		}
		assertpro(mvpp <= mvallistend);
		lclp = lcllist + inp->position - 1;
		if (lclp->initted)
			rts_error(VARLSTCNT(5) ERR_ZCALLTABLE, 0, ERR_ZCPOSOVR, 1, inp->position);
		lclp->skip = FALSE;
		lclp->zctab = inp;
		if (ZC$MECH_DESCRIPTOR64 == inp->mechanism)
		{
			INIT_64BITDESC;
		} else
		{
			INIT_32BITDESC;
		}
		use_value = VAL_NONE;
		switch (inp->qualifier)
		{
			case ZC$IQUAL_CONSTANT:
				use_value = VAL_ZCTABVAL;
				break;
			case ZC$IQUAL_OPTIONAL:
				if (mvpp == mvallistend)
					lclp->skip = TRUE;
				else if (!MV_DEFINED(*mvpp) && (*mvpp)->str.addr == (*mvpp))
				{
					lclp->skip = TRUE;
					mvpp++;
				} else
					use_value = VAL_INPUTMVAL;
				break;
			case ZC$IQUAL_OPTIONAL_0:
				if (mvpp == mvallistend || (!MV_DEFINED(*mvpp) && (*mvpp)->str.addr == (*mvpp)))
				{
					if (!(ZC$MECH_REFERENCE == inp->mechanism ||
					      ZC$MECH_DESCRIPTOR == inp->mechanism || ZC$MECH_DESCRIPTOR64 == inp->mechanism))
						rts_error(VARLSTCNT(3) ERR_ZCALLTABLE, 0, ERR_ZCOPT0);
					lclp->skip = TRUE;
					if (is_a_desc64)
						dsc64->dsc64$pq_pointer = 0;
					else
						dsc->dsc$a_pointer = 0;
					if (mvpp < mvallistend)
						mvpp++;
				} else
					use_value = VAL_INPUTMVAL;
				break;
			case ZC$IQUAL_DEFAULT:
				if (mvpp == mvallistend)
					use_value = VAL_ZCTABVAL;
				else if (!MV_DEFINED(*mvpp) && (*mvpp)->str.addr == (*mvpp))
				{
					use_value = VAL_ZCTABVAL;
					mvpp++;
				} else
					use_value = VAL_INPUTMVAL;
				break;
			case ZC$IQUAL_REQUIRED:
				if (mvpp == mvallistend || (!MV_DEFINED(*mvpp) && (*mvpp)->str.addr == (*mvpp)))
					rts_error(VARLSTCNT(1) ERR_ZCINPUTREQ);
				use_value = VAL_INPUTMVAL;
				break;
			default:
				rts_error(VARLSTCNT(3) ERR_ZCALLTABLE, 0, ERR_ZCUNKQUAL);
		}
		switch (use_value)
		{
			case VAL_INPUTMVAL:
				if (!is_a_desc64)
					dsc->dsc$w_length = 0;
				else
					dsc64->dsc64$q_length = 0;
				*class = DSC$K_CLASS_S;
				*type = inp->type;
				if (ZC$DTYPE_STRING == inp->type)
				{
					MV_FORCE_STR(*mvpp);
					if (!is_a_desc64)
					{
						if (65535 < (*mvpp)->str.len)
							rts_error(VARLSTCNT(1) ERR_ZCWRONGDESC);
						dsc->dsc$w_length =(*mvpp)->str.len ;
						dsc->dsc$a_pointer = (*mvpp)->str.addr;
					} else
					{
						dsc64->dsc64$q_length = (*mvpp)->str.len;
						dsc64->dsc64$pq_pointer = (*mvpp)->str.addr;
					}
				} else
				{
					if (!is_a_desc64)
					{
						dsc->dsc$w_length = dsizes[inp->type];
						dsc->dsc$a_pointer = &lclp->data;
					} else
					{
						dsc64->dsc64$q_length = dsizes[inp->type];
						dsc64->dsc64$pq_pointer = (char *)&lclp->data;
					}
					assert(is_a_desc64 ? dsc64->dsc64$q_length : dsc->dsc$w_length);
					mval2desc(*mvpp, &lclp->dsc);
				}
				mvpp++;
				break;
			case VAL_ZCTABVAL:
				if (ZC$DTYPE_STRING == inp->type)
				{
					/*
					  for this case inp->value cannot be a 64-bit descriptor, inp->value is the descriptor
					  created by putval macro in gtmzcall.max. putval creates a descriptor for each input
					  string value by using the directive .ascid. So for descriptor64, we only copy the
					  32-bit contents to dsc64.
					*/
					if (is_a_desc64)
					{
						valdsc = (struct dsc$descriptor *)inp->value;
						dsc64->dsc64$w_mbo = 1;
						dsc64->dsc64$b_dtype = valdsc->dsc$b_dtype;
						dsc64->dsc64$b_class = valdsc->dsc$b_class;
						dsc64->dsc64$l_mbmo = -1;
						dsc64->dsc64$q_length = valdsc->dsc$w_length;
						dsc64->dsc64$pq_pointer = valdsc->dsc$a_pointer;
					}
					else
						*dsc = *(struct dsc$descriptor *)inp->value;
				} else
				{
					*type = inp->type;
					*class = DSC$K_CLASS_S;
					if (!is_a_desc64)
					{
						dsc->dsc$w_length = dsizes[inp->type];
						dsc->dsc$a_pointer = &lclp->data;
						memcpy(dsc->dsc$a_pointer, inp->value, dsizes[inp->type]);
					} else
					{
						dsc64->dsc64$q_length = dsizes[inp->type];
						dsc64->dsc64$pq_pointer = (char *)&lclp->data;
						memcpy(dsc64->dsc64$pq_pointer, inp->value, dsizes[inp->type]);
					}
				}
				break;
			case VAL_NONE:
				break;
			default:
				GTMASSERT;
				break;		/* though not necessary, keep compiler on some platforms happy */
		}
		lclp->initted = TRUE;
	}
	assert(inp == (zctabinput *)firstout);
	if (mvpp < mvallistend) rts_error(VARLSTCNT(1) ERR_ZCCONMSMTCH);
	assert(mvpp == mvallistend);
	is_a_desc64 = FALSE;
	for (outp = firstout; outp < lastout; outp++)
	{
		switch (outp->type)	/* guard type */
		{
			case ZC$DTYPE_STRING:
			case ZC$DTYPE_BYTE:
			case ZC$DTYPE_BYTEU:
			case ZC$DTYPE_WORD:
			case ZC$DTYPE_WORDU:
			case ZC$DTYPE_LONG:
			case ZC$DTYPE_LONGU:
			case ZC$DTYPE_QUAD:
			case ZC$DTYPE_FLOATING:
			case ZC$DTYPE_DOUBLE:
			case ZC$DTYPE_H_FLOATING:
				break;
			default:
				rts_error(VARLSTCNT(1) ERR_ZCUNKTYPE);
		}
		switch (outp->mechanism)	/* guard mechanism */
		{
			case ZC$MECH_VALUE:
			case ZC$MECH_REFERENCE:
			case ZC$MECH_DESCRIPTOR:
			case ZC$MECH_DESCRIPTOR64:
				break;
			default:
				rts_error(VARLSTCNT(1) ERR_ZCUNKMECH);
		}
		switch (outp->qualifier)	/* guard qualifier */
		{
			case ZC$OQUAL_REQUIRED:
			case ZC$OQUAL_DUMMY:
			case ZC$OQUAL_PREALLOCATE:
				break;
			default:
				rts_error(VARLSTCNT(3) ERR_ZCALLTABLE, 0, ERR_ZCUNKQUAL);
		}
		lclp = lcllist + outp->position - 1;
		if (lclp->initted)
		{
			inp = lclp->zctab;
			if (lclp->skip || outp->type != inp->type || outp->mechanism != inp->mechanism ||
			    inp->type == ZC$DTYPE_STRING || outp->qualifier == ZC$OQUAL_PREALLOCATE)
				rts_error(VARLSTCNT(5) ERR_ZCALLTABLE, 0, ERR_ZCPOSOVR, 1, outp->position);
		} else
		{
			lclp->skip = FALSE;
			lclp->zctab = outp;
			if (ZC$MECH_DESCRIPTOR64 == outp->mechanism)
			{
				INIT_64BITDESC;
			} else
			{
				INIT_32BITDESC;
			}
			*class = DSC$K_CLASS_S;
			*type = outp->type;
			if (outp->type == ZC$DTYPE_STRING)
			{
				assert(ZC$MECH_DESCRIPTOR == outp->mechanism || ZC$MECH_DESCRIPTOR64 == outp->mechanism );
				if (!is_a_desc64)
				{
					dsc->dsc$w_length = 0;
					dsc->dsc$a_pointer = 0;
				} else
				{
					dsc64->dsc64$q_length = 0;
					dsc64->dsc64$pq_pointer = 0;
				}
				if (ZC$OQUAL_PREALLOCATE == outp->qualifier && dst)
				{	/* if no output string, ignore this, else allocate space for the output string */
					assert((0 < outp->value) && (outp->value <= MAX_STRLEN));
					alloclen = outp->value;
					if (is_a_desc64)
					{
						val_qw = alloclen;
						status = lib$sget1_dd_64(&val_qw, dsc64);
					} else
					{
						val_us = alloclen;
						status = lib$sget1_dd(&val_us, dsc);
					}
					if (!(status & 1))
					{
						if (LIB$_INSVIRMEM == status)
							rts_error(VARLSTCNT(3) ERR_VMSMEMORY2, 1, alloclen);
						else
						{
							assert(LIB$_FATERRLIB == status);
							GTMASSERT;	/* Only other return code is fatal internal error */
						}
					}
				} else
				{
					assert(0 == outp->value);
					*class = DSC$K_CLASS_D;
				}
			} else
			{
				if (!is_a_desc64)
				{
					dsc->dsc$w_length = dsizes[inp->type];
					dsc->dsc$a_pointer = &lclp->data;
				} else
				{
					dsc64->dsc64$q_length = dsizes[inp->type];
					dsc64->dsc64$pq_pointer = (char *)&lclp->data;
				}
				assert(is_a_desc64 ? dsc64->dsc64$q_length : dsc->dsc$w_length);
			}
		}
	}
	assert(outp == lastout);
	if (zcrtn->outbnd_reset)	/* if indicated, disable our outofband handling */
	{
		if (io_std_device.in->type == tt)
			resetterm(io_std_device.in);
		zc_call(zcrtn, zcret, lcllist, lcllistend, &save_ret);
		if (io_std_device.in->type == tt)
			setterm(io_std_device.in);
	} else
		zc_call(zcrtn, zcret, lcllist, lcllistend, &save_ret);
	VERIFY_STORAGE_CHAINS;
	if (mask)
	{
		for (indx = 0;; indx++)
		{
			if (!mask)
				break;
			byref = mask & 1;
			mask >>= 1;
			if (!byref)
				continue;
			inp = firstin + indx;
			lclp = lcllist + inp->position - 1;
			if (!lclp->skip)
			{
				desc2mval(&lclp->dsc, &tmpmval);
				mvpp = mvallist + indx;
				**mvpp = tmpmval;
			}
		}
	}
	if (dst != NULL)	/* if a return value expected */
	{	/* Allocate space in stringpool for destination mval */
		dst->mvtype = 0;
		dstlen = 0;
		if (ZC$RETC_VALUE == zcret->class)
			dstlen += MAXNUMLEN;
		for (outp = firstout; outp < lastout; outp++)
		{
			if (ZC$OQUAL_DUMMY != outp->qualifier)
			{
				if (dstlen)
					dstlen += 1;
				lclp = lcllist + outp->position - 1;
				if ($is_desc64(&lclp->dsc))
				{
					INIT_64BITDESC;
				} else
				{
					INIT_32BITDESC;
				}
				if (DSC$K_DTYPE_T != *type)
					dstlen += MAXNUMLEN;
				else
				{
					if (!is_a_desc64)
						dstlen += dsc->dsc$w_length;
					else {
						if (MAX_STRLEN < dsc64->dsc64$q_length)
							rts_error(VARLSTCNT(1) ERR_MAXSTRLEN);
						dstlen += dsc64->dsc64$q_length;
					}
				}
			}
		}
		if (MAX_STRLEN < dstlen)
			rts_error(VARLSTCNT(1) ERR_MAXSTRLEN);
		ENSURE_STP_FREE_SPACE(dstlen);
		/* Construct destination mval */
		dst->str.addr = stringpool.free;
		dstlen = 0;
		if (ZC$RETC_VALUE == zcret->class)
		{
			tmpdsc.dsc$b_dtype = zcret->type;
			tmpdsc.dsc$b_class = DSC$K_CLASS_S;
			tmpdsc.dsc$a_pointer = &save_ret;
			desc2mval(&tmpdsc, &tmpmval);
			MV_FORCE_STRD(&tmpmval);
			dstlen += tmpmval.str.len;
			*stringpool.free++ = ',';
			dstlen++;
		}
		for (outp = firstout; outp < lastout; outp++)
		{
			if (ZC$OQUAL_DUMMY != outp->qualifier)
			{
				lclp = lcllist + outp->position - 1;
				if ($is_desc64(&lclp->dsc))
				{
					INIT_64BITDESC;
				} else
				{
					INIT_32BITDESC;
				}
				if (DSC64$K_DTYPE_T == *type)
				{
					if (!is_a_desc64)
					{
						memcpy(stringpool.free, dsc->dsc$a_pointer, dsc->dsc$w_length);
						stringpool.free += dsc->dsc$w_length;
						dstlen += dsc->dsc$w_length;
					} else
					{
						memcpy(stringpool.free, dsc64->dsc64$pq_pointer, dsc64->dsc64$q_length);
						stringpool.free += dsc64->dsc64$q_length;
						dstlen += dsc64->dsc64$q_length;
					}
					if (DSC64$K_CLASS_S == *class && ZC$OQUAL_PREALLOCATE == outp->qualifier)
					{
						if (!is_a_desc64)
							dsc->dsc$w_length = outp->value;
						else
							dsc64->dsc64$q_length = outp->value;
						if ((status = lib$sfree1_dd(&lclp->dsc)) != SS$_NORMAL)
							rts_error(VARLSTCNT(3) ERR_ZCCONVERT, 0, status);
					} else if (DSC64$K_CLASS_D == *class && (dsc->dsc$a_pointer || dsc64->dsc64$pq_pointer))
					{
						if ((status = lib$sfree1_dd(&lclp->dsc)) != SS$_NORMAL)
							rts_error(VARLSTCNT(3) ERR_ZCCONVERT, 0, status);
					}
				} else
				{
					desc2mval(&lclp->dsc, &tmpmval);
					MV_FORCE_STRD(&tmpmval);
					dstlen += tmpmval.str.len;
				}
				*stringpool.free++ = ',';
				dstlen++;
			}
		}
		dst->mvtype = MV_STR;
		if (dstlen == 0)
		{
			assert(stringpool.free == dst->str.addr);
			dst->str.len = 0;
		} else
		{
			stringpool.free--;	/* take off trailing , */
			dst->str.len = stringpool.free - (unsigned char *)dst->str.addr;
			assert(dst->str.len == dstlen - 1);
		}
	}
	return;
}
