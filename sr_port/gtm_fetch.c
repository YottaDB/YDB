/****************************************************************
 *								*
 *	Copyright 2009, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stdarg.h>

#include <rtnhdr.h>
#include "stack_frame.h"
#include "lookup_variable_htent.h"
#include "op.h"
#include "lv_val.h"

#ifdef DEBUG	/* all of below is needed by the gv_target/cs_addrs assert */
#include "gdsroot.h"
#include "gdskill.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#endif

GBLREF stack_frame	*frame_pointer;
GBLREF symval           *curr_symval;
#ifdef DEBUG
GBLREF int		process_exiting;
GBLREF gv_namehead	*gv_target;
GBLREF sgmnt_addrs	*cs_addrs;
#endif

#ifdef UNIX
void gtm_fetch(unsigned int cnt_arg, unsigned int indxarg, ...)
#elif defined(VMS)
void gtm_fetch(unsigned int indxarg, ...)
#else
#error unsupported platform
#endif
{
	va_list		var;
	unsigned int 	indx;
	unsigned int 	cnt;
	stack_frame	*fp;
	ht_ent_mname	**htepp;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(!process_exiting);	/* Verify that no process unwound the exit frame and continued */
	DEBUG_ONLY(DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC(CHECK_CSA_TRUE);) /* surrounding DEBUG_ONLY needed because gdsfhead.h is
									   * not included for pro builds and so the macro and its
									   * parameters would be undefined in that case causing a
									   * compile-time error.
									   */
	assert(!TREF(in_zwrite));	/* Verify in_zwrite was not left on */
	VAR_START(var, indxarg);
	VMS_ONLY(va_count(cnt);)
	UNIX_ONLY(cnt = cnt_arg;)	/* need to preserve stack copy on i386 */
	fp = frame_pointer;
	if (0 < cnt)
	{	/* All generated code comes here to verify instantiation
		   of a given set of variables from the local variable table */
		indx = indxarg;
		for ( ; ; )
		{
			htepp = &fp->l_symtab[indx];
			if (NULL == *htepp)
				*htepp = lookup_variable_htent(indx);
			assert(NULL != (*htepp)->value);
			assert(LV_IS_BASE_VAR((*htepp)->value));
			assert(NULL != LV_SYMVAL((lv_val *)((*htepp)->value)));
			if (0 < --cnt)
				indx = va_arg(var, int4);
			else
				break;
		}
	} else
	{	/* GT.M calls come here to verify instantiation of the
		   entire local variable table */
		indx = fp->vartab_len;
		htepp = &fp->l_symtab[indx];
		for (; indx > 0;)
		{
			--indx;
			--htepp;
			if (NULL == *htepp)
				*htepp = lookup_variable_htent(indx);
			else if (NULL == (*htepp)->value)
				lv_newname(*htepp, curr_symval);	/* Alias processing may have removed the lv_val */
		}
	}
	va_end(var);
}
