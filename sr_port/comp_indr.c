/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_string.h"

#include <rtnhdr.h>
#include "stack_frame.h"
#include "mv_stent.h"
#include "copy.h"
#include "cache.h"
#include "objlabel.h"
#include "mprof.h"
#include "compiler.h"
#include "obj_file.h"
#include "error.h"

GBLREF mv_stent		*mv_chain;
GBLREF unsigned char	*stackbase, *stacktop, *stackwarn, *msp;
GBLREF stack_frame	*frame_pointer;
GBLREF boolean_t	is_tracing_on;

error_def(ERR_STACKCRIT);
error_def(ERR_STACKOFLOW);

void comp_indr(mstr *obj)
{
	stack_frame	*sf;
	unsigned char	*fix, *fix_base, *tmps, *syms, *save_msp;
	int		tempsz, vartabsz, fixup_cnt, zapsz;
	INTPTR_T	*vp;
	ihdtyp		*rtnhdr;

	assert((frame_pointer < frame_pointer->old_frame_pointer) || (NULL == frame_pointer->old_frame_pointer));
	save_msp = msp;
	sf = (stack_frame *)(msp -= SIZEOF(stack_frame));
	rtnhdr = (ihdtyp *)obj->addr;
	/* Check that our cache_entry pointer is in proper alignment with us */
	assert(rtnhdr->indce->obj.addr == (char *)rtnhdr);
	tempsz = ROUND_UP2(rtnhdr->temp_size, SIZEOF(char *));
	tmps = msp -= tempsz;
	vartabsz = rtnhdr->vartab_len;
	vartabsz *= SIZEOF(ht_ent_mname *);
	/* Check that our vars and friends can fit on this stack */
	if ((msp -= vartabsz) <= stackwarn)
	{
		if (msp <= stacktop)
		{
			msp = save_msp;
			rts_error(VARLSTCNT(1) ERR_STACKOFLOW);
		} else
			rts_error(VARLSTCNT(1) ERR_STACKCRIT);
	}
	syms = msp;
	*sf = *frame_pointer;
	sf->old_frame_pointer = frame_pointer;
	sf->type = 0;
	sf->temps_ptr = tmps;
	sf->l_symtab = (ht_ent_mname **)syms;
	sf->vartab_len = rtnhdr->vartab_len;
	if (zapsz = (vartabsz + tempsz))	/* Note assignment */
		memset(syms, 0, zapsz);		/* Zap temps and symtab together */
	sf->vartab_ptr = (char *)rtnhdr + rtnhdr->vartab_off;
	sf->temp_mvals = rtnhdr->temp_mvals;
	/* Code starts just past the literals that were fixed up and past the validation and hdr offset fields */
	sf->mpc = (unsigned char *)rtnhdr + rtnhdr->fixup_vals_off + (rtnhdr->fixup_vals_num * SIZEOF(mval));
	/* IA64 required SECTION_ALIGN_BOUNDARY alignment (16 bytes). ABS 2008/12
	 * This has been carried forward to other 64bit platfoms without problems
	 */
        GTM64_ONLY(sf->mpc = (unsigned char *)ROUND_UP2((UINTPTR_T)sf->mpc, SECTION_ALIGN_BOUNDARY));
	sf->mpc = sf->mpc + (2 * SIZEOF(INTPTR_T)); /* Account for hdroffset and MAGIC_VALUE */
	sf->flags = SFF_INDCE;		/* We will be needing cleanup for this frame */
	sf->ret_value = NULL;
	sf->dollar_test = -1;		/* initialize it with -1 for indication of not yet being used */
	DEBUG_ONLY(
		vp = (INTPTR_T *)sf->mpc;
		assert(NULL != vp);
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
	DBGEHND((stderr, "comp_indr: Added indirect stack frame at addr 0x"lvaddr" - New msp: 0x"lvaddr"\n", sf, msp));
	assert((frame_pointer < frame_pointer->old_frame_pointer) || (NULL == frame_pointer->old_frame_pointer));
	return;
}
