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

/* map_sym.c VMS - load function from shared library for various collation symbols */
/* Return TRUE/FALSE based on mapping success */

#include <descrip.h>
#include <ssdef.h>

#include "mdef.h"
#include "error.h"
#include "collseq.h"
#include "gtmmsg.h"

#define CHECK_ERR_STAT		\
{				\
	REVERT;			\
	return 	FALSE;		\
}

STATICFNDCL CONDITION_HANDLER(map_sym_ch);

error_def(ERR_ASSERT);
error_def(ERR_COLLFNMISSING);
error_def(ERR_GTMASSERT);
error_def(ERR_GTMASSERT2);
error_def(ERR_GTMCHECK);
error_def(ERR_STACKOFLOW);
error_def(ERR_VMSMEMORY);

boolean_t map_collseq(mstr *fspec, collseq *ret_collseq)
{
	struct dsc$descriptor	fspec_desc;
	struct dsc$descriptor	symbol_desc;
	int			status;
	boolean_t		coll_lib_found = FALSE;
	ch_ret_type		map_sym_ch();
        static MSTR_CONST(xform_sym_1, "gtm_ac_xform_1");
        static MSTR_CONST(xback_sym_1, "gtm_ac_xback_1");
        static MSTR_CONST(xform_sym, "gtm_ac_xform");
        static MSTR_CONST(xback_sym, "gtm_ac_xback");
        static MSTR_CONST(verify_sym, "gtm_ac_verify");
        static MSTR_CONST(version_sym, "gtm_ac_version");

	ESTABLISH(map_sym_ch);
	fspec_desc.dsc$w_length = fspec->len;
	fspec_desc.dsc$b_dtype = DSC$K_DTYPE_T;
	fspec_desc.dsc$b_class = DSC$K_CLASS_S;
	fspec_desc.dsc$a_pointer = fspec->addr;
	symbol_desc.dsc$w_length = xform_sym_1.len;
	symbol_desc.dsc$b_dtype = DSC$K_DTYPE_T;
	symbol_desc.dsc$b_class = DSC$K_CLASS_S;
	symbol_desc.dsc$a_pointer = xform_sym_1.addr;
	status = lib$find_image_symbol(&fspec_desc, &symbol_desc, &(ret_collseq->xform), 0);
	if (status & 1)
	{
		symbol_desc.dsc$w_length = xback_sym_1.len;
		symbol_desc.dsc$b_dtype = DSC$K_DTYPE_T;
		symbol_desc.dsc$b_class = DSC$K_CLASS_S;
		symbol_desc.dsc$a_pointer = xback_sym_1.addr;
		status = lib$find_image_symbol(&fspec_desc, &symbol_desc, &(ret_collseq->xback), 0);
		if (status & 1)
		{
			coll_lib_found = TRUE;
			ret_collseq->argtype = 1;
		} else
		{
			gtm_putmsg(VARLSTCNT(5) ERR_COLLFNMISSING, 3, LEN_AND_LIT("gtm_ac_xback_1()"), ret_collseq->act );
			CHECK_ERR_STAT;
		}
	}
	if (FALSE == coll_lib_found)
	{
		symbol_desc.dsc$w_length = xform_sym.len;
		symbol_desc.dsc$b_dtype = DSC$K_DTYPE_T;
		symbol_desc.dsc$b_class = DSC$K_CLASS_S;
		symbol_desc.dsc$a_pointer = xform_sym.addr;
		status = lib$find_image_symbol(&fspec_desc, &symbol_desc, &(ret_collseq->xform), 0);
		if (status & 1)
		{
			symbol_desc.dsc$w_length = xback_sym.len;
			symbol_desc.dsc$b_dtype = DSC$K_DTYPE_T;
			symbol_desc.dsc$b_class = DSC$K_CLASS_S;
			symbol_desc.dsc$a_pointer = xback_sym.addr;
			status = lib$find_image_symbol(&fspec_desc, &symbol_desc, &(ret_collseq->xback), 0);
			if (status & 1)
			{
				coll_lib_found = TRUE;
				ret_collseq->argtype = 0;
			} else
			{
				gtm_putmsg(VARLSTCNT(5) ERR_COLLFNMISSING, 3, LEN_AND_LIT("gtm_ac_xback()"), ret_collseq->act );
				CHECK_ERR_STAT;
			}
		} else /* Neither xform_1 or xform is found */
			CHECK_ERR_STAT;
	}
	assert(TRUE == coll_lib_found);
	symbol_desc.dsc$w_length = verify_sym.len;
	symbol_desc.dsc$b_dtype = DSC$K_DTYPE_T;
	symbol_desc.dsc$b_class = DSC$K_CLASS_S;
	symbol_desc.dsc$a_pointer = verify_sym.addr;
	status = lib$find_image_symbol(&fspec_desc, &symbol_desc, &(ret_collseq->verify), 0);
	if (!(status & 1))
		CHECK_ERR_STAT;

	symbol_desc.dsc$w_length = version_sym.len;
	symbol_desc.dsc$b_dtype = DSC$K_DTYPE_T;
	symbol_desc.dsc$b_class = DSC$K_CLASS_S;
	symbol_desc.dsc$a_pointer = version_sym.addr;
	status = lib$find_image_symbol(&fspec_desc, &symbol_desc, &(ret_collseq->version), 0);
	if (!(status & 1))
		CHECK_ERR_STAT;
	REVERT;
	return TRUE;
}

STATICFNDEF CONDITION_HANDLER(map_sym_ch)
{
	int4 	status;

	START_CH;

	if (DUMP)
		NEXTCH;
	mch->CHF_MCH_SAVR0 = SIGNAL;	/* return status from lib$find_image_symbol to map_sym */
	if ((status = sys$unwind(&mch->CHF_MCH_DEPTH, 0)) != SS$_NORMAL)
		NEXTCH;
}
