/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "rtnhdr.h"
#include "stack_frame.h"
#include "mv_stent.h"
#include "copy.h"
#include "cache.h"
#include "objlabel.h"
#include "mprof.h"
#include "cacheflush.h"
#include "compiler.h"
#include "obj_file.h"

GBLREF mv_stent		*mv_chain;
GBLREF unsigned char	*stackbase, *stacktop, *stackwarn, *msp;
GBLREF stack_frame	*frame_pointer;
GBLREF boolean_t	is_tracing_on;

void	comp_indr (mstr *obj)
{
	stack_frame	*sf;
	unsigned char	*fix, *fix_base, *tmps, *syms, *save_msp;
	int		tempsz, vartabsz, fixup_cnt;
	INTPTR_T	*vp;
	ihdtyp		*rtnhdr;
	error_def(ERR_STACKOFLOW);
	error_def(ERR_STACKCRIT);

	save_msp = msp;
	sf = (stack_frame *)(msp -= sizeof(stack_frame));
	rtnhdr = (ihdtyp *)obj->addr;

	/* Check that our cache_entry pointer is in proper alignment with us */
	assert(rtnhdr->indce->obj.addr == (char *)rtnhdr);

	tempsz = ROUND_UP2(rtnhdr->temp_size, SIZEOF(char *));
	tmps = msp -= tempsz;
	vartabsz = rtnhdr->vartab_len;
	vartabsz *= sizeof(mval *);
	/* Check that our vars and friends can fit on this stack */
	if ((msp -= vartabsz) <= stackwarn)
	{
		if (msp <= stacktop)
		{
			msp  = save_msp;
			rts_error(VARLSTCNT(1) ERR_STACKOFLOW);
		}
		else
			rts_error(VARLSTCNT(1) ERR_STACKCRIT);
	}
	syms = msp;

	*sf = *frame_pointer;
	sf->old_frame_pointer = frame_pointer;
	sf->type = 0;
	sf->temps_ptr = tmps;
	if (tempsz)
		memset(tmps, 0, tempsz);
	sf->l_symtab = (mval **)syms;
	sf->vartab_len = rtnhdr->vartab_len;
	if (vartabsz)
		memset(syms, 0, vartabsz);

	sf->vartab_ptr = (char *)rtnhdr + rtnhdr->vartab_off;
	sf->temp_mvals = rtnhdr->temp_mvals;
	/* Code starts just past the literals that were fixed up and past the validation and hdr offset fields */
	sf->mpc = (unsigned char *)rtnhdr + rtnhdr->fixup_vals_off + (rtnhdr->fixup_vals_num * sizeof(mval));
        GTM64_ONLY(sf->mpc  = (unsigned char *)ROUND_UP2((UINTPTR_T)sf->mpc, SECTION_ALIGN_BOUNDARY));
	sf->mpc = sf->mpc + (2 * sizeof(INTPTR_T)); /*Account for hdroffset and MAGIC_VALUE*/
	sf->flags = SFF_INDCE;		/* We will be needing cleanup for this frame */
	DEBUG_ONLY(
		vp = (INTPTR_T *)sf->mpc;
		vp--;
		assert((GTM_OMAGIC << 16) + OBJ_LABEL == *vp);
		vp--;
		assert((unsigned char*)rtnhdr == (unsigned char *)vp + *vp);
	);
	rtnhdr->indce->refcnt++;	/* This entry is now in use on M stack */

	if (is_tracing_on)
		new_prof_frame(FALSE);
	sf->ctxt = sf->mpc;
	assert(msp < stackbase);
	frame_pointer = sf;
	return;
}
