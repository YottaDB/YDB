/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "error.h"
#include "indir_enum.h"
#include "rtnhdr.h"
#include "op.h"
#include "stack_frame.h"
#include "hashtab_mname.h"	/* needed for lv_val.h */
#include "lv_val.h"
#include "mv_stent.h"
#include "gtm_stdio.h"
#include "jobinterrupt_process.h"

GBLREF stack_frame	*frame_pointer;
GBLREF unsigned char	*stackbase, *stacktop, *msp, *stackwarn;
GBLREF mval		dollar_zinterrupt;
GBLREF boolean_t	dollar_zininterrupt;
GBLREF unsigned short	proc_act_type;
GBLREF mv_stent		*mv_chain;
GBLREF int		dollar_truth;
GBLREF mstr		extnam_str;

void jobinterrupt_process(void)
{
	error_def(ERR_STACKOFLOW);
	error_def(ERR_STACKCRIT);

	assert(dollar_zininterrupt);
	/* Compile and push new (counted) frame onto the stack to drive the
	   $zinterrupt handler.
	*/
	assert((SFT_COUNT | SFT_ZINTR) == proc_act_type);
	op_commarg(&dollar_zinterrupt, indir_linetail);
	frame_pointer->type = proc_act_type;	/* The mark of zorro.. */
	proc_act_type = 0;

	/* Now that the frame is set up, we need to preserve our current environment. This
	   is a job for MVST_ZINTR which will hold the current dollar_truth value and
	   provide an mval for op_gvrectarg to save the current key info into. When this
	   entry is popped off the stack, these values will be restored. Note that we are
	   specifically NOT saving/restoring the current I/O device here. The reason is we
	   were able to envision uses for job interrupt that might involve legitimate changes
	   in the current IO device (for example application log switches). So in the
	   interest of flexibilty, we leave this big gun pointed at the user's feet..
	   2006-03-06 se: add any extended reference in $REFERENCE to the things being saved/restored
	*/
	PUSH_MV_STENT(MVST_ZINTR);
	mv_chain->mv_st_cont.mvs_zintr.saved_dollar_truth = dollar_truth;
	op_gvsavtarg(&mv_chain->mv_st_cont.mvs_zintr.savtarg);
	mv_chain->mv_st_cont.mvs_zintr.savextref.len = extnam_str.len;
	if (extnam_str.len)
	{
		mv_chain->mv_st_cont.mvs_zintr.savextref.addr = (char *)malloc(extnam_str.len);
		memcpy(mv_chain->mv_st_cont.mvs_zintr.savextref.addr, extnam_str.addr, extnam_str.len);
	} else
		mv_chain->mv_st_cont.mvs_zintr.savextref.addr = NULL;
	return;
}
