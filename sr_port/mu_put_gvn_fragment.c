/****************************************************************
 *                                                              *
 *      Copyright 2012 Fidelity Information Services, Inc       *
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

#include "mdef.h"
#include "op.h"
#include "mupip_put_gvn_fragment.h"
#include "mv_stent.h"

void mupip_put_gvn_fragment(char *cp,int len, int val_off1, int val_len1)
{
	mval 		*u, v;
	mv_stent	*mv_stacktop;

	PUSH_MV_STENT(MVST_MVAL);       /* protect mval value from stp_gcol */
	mv_stacktop = mv_chain;
	u = &mv_chain->mv_st_cont.mvs_mval;
	u->str.len = 0;
	u->mvtype = 0;
	op_fngvget1(u);
	v.mvtype = MV_STR;
	v.str.addr = cp;
	v.str.len = len;
	op_setzextract(u, &v, val_off1+1, (val_off1 + val_len1), u);
	op_gvput(u);
	assertpro(mv_stacktop == mv_chain);
	POP_MV_STENT();
	return;
}
