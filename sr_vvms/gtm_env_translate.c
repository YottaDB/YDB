/****************************************************************
 *								*
 *	Copyright 2006, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include <descrip.h>
#include <stddef.h>
#include <ssdef.h>
#include "error.h"
#include "gtm_env_xlate_init.h"

GBLREF mstr	env_gtm_env_xlate;
GBLREF mval	dollar_zdir;

error_def(ERR_ASSERT);
error_def(ERR_GTMASSERT);
error_def(ERR_GTMASSERT2);
error_def(ERR_GTMCHECK);
error_def(ERR_STACKOFLOW);
error_def(ERR_TEXT);
error_def(ERR_VMSMEMORY);
error_def(ERR_XTRNRETSTR);
error_def(ERR_XTRNRETVAL);
error_def(ERR_XTRNTRANSDLL);
error_def(ERR_XTRNTRANSDLL);
error_def(ERR_XTRNTRANSERR);

/* This condition handler is necessary since execution does not return to
 * gtm_env_translate if there is an error in lib$find_image_symbol otherwise */
CONDITION_HANDLER(gtm_env_xlate_ch)
{
	int4    status;

	START_CH;
	PRN_ERROR;
	if (DUMP)
		NEXTCH;
	mch->CHF_MCH_SAVR0 = SIGNAL;	/* return status from lib$find_image_symbol to op_gvextnam or mlk_pvtblk_create*/
	if ((status = sys$unwind(&mch->CHF_MCH_DEPTH, 0)) != SS$_NORMAL)
		NEXTCH;
}

mval* gtm_env_translate(mval* val1, mval* val2, mval* val_xlated)
{
	int			ret_gtm_env_xlate;
	int4			status;
	struct dsc$descriptor	filename;
	struct dsc$descriptor	entry_point;
	MSTR_CONST(routine_name, GTM_ENV_XLATE_ROUTINE_NAME);
	/* The env xlate routines expect parameters of type xc_string_t* which
	 * is not defined on VMS. On VMS, xc_string_t is assumed to be same as
	 * mstr. Following code is to ensure any future mstr changes are caught
	 * sooner than later */
	typedef struct
	{
		unsigned int	length;
		char*		address;
	} xc_string_t;	/* A replica of what mstr ought to be */
	xc_string_t*	dummy_ptr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(offsetof(xc_string_t, length) == offsetof(mstr, len) && SIZEOF(dummy_ptr->length) == SIZEOF(val1->str.len));
	assert(offsetof(xc_string_t, address) == offsetof(mstr, addr) && SIZEOF(dummy_ptr->address) == SIZEOF(val1->str.addr));
	if (0 != env_gtm_env_xlate.len)
	{
		MV_FORCE_STR(val2);
		if (NULL == RFPTR(gtm_env_xlate_entry))
		{
			entry_point.dsc$b_dtype = DSC$K_DTYPE_T;
			entry_point.dsc$b_class = DSC$K_CLASS_S;
			entry_point.dsc$w_length = routine_name.len;
			entry_point.dsc$a_pointer = routine_name.addr;
			filename.dsc$b_dtype = DSC$K_DTYPE_T;
			filename.dsc$b_class = DSC$K_CLASS_S;
			filename.dsc$w_length = env_gtm_env_xlate.len;
			filename.dsc$a_pointer = env_gtm_env_xlate.addr;

			ESTABLISH(gtm_env_xlate_ch);
			status = lib$find_image_symbol(&filename, &entry_point, &RFPTR(gtm_env_xlate_entry), 0);
			REVERT;
			if (0 == (status & 1))
				rts_error(VARLSTCNT(1) ERR_XTRNTRANSDLL);
		}
		val_xlated->str.addr = NULL;
		ret_gtm_env_xlate = IVFPTR(gtm_env_xlate_entry)(&val1->str, &val2->str, &dollar_zdir.str, &val_xlated->str);
		if (MAX_DBSTRLEN < val_xlated->str.len)
			rts_error(VARLSTCNT(4) ERR_XTRNRETVAL, 2, val_xlated->str.len, MAX_DBSTRLEN);
		if (0 != ret_gtm_env_xlate)
		{
			if ((val_xlated->str.len) && (val_xlated->str.addr))
				rts_error(VARLSTCNT(6) ERR_XTRNTRANSERR, 0, ERR_TEXT,  2,
					  val_xlated->str.len, val_xlated->str.addr);
			else
				rts_error(VARLSTCNT(1) ERR_XTRNTRANSERR);
		}
		if ((NULL == val_xlated->str.addr) && (0 != val_xlated->str.len))				\
			rts_error(VARLSTCNT(1) ERR_XTRNRETSTR);
		val_xlated->mvtype = MV_STR;
		val1 = val_xlated;
	}
	return val1;
}
