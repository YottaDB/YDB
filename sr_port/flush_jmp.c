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

#include "gtm_stdio.h"
#include "gtm_string.h"

#include <rtnhdr.h>
#include "mv_stent.h"
#include "objlabel.h"
#include "cache.h"
#include "stack_frame.h"
#include "cache_cleanup.h"
#include "op.h"
#include "unwind_nocounts.h"
#include "flush_jmp.h"
#include "error.h"
#include "tp_frame.h"
#ifdef GTM_TRIGGER
# include "gtm_trigger_trc.h"
#endif

GBLREF	symval		*curr_symval;
GBLREF	stack_frame	*error_frame;
GBLREF	stack_frame	*frame_pointer;
GBLREF	mv_stent	*mv_chain;
GBLREF	unsigned char	*stackbase,*stacktop,*msp,*stackwarn;
GBLREF	tp_frame	*tp_pointer;

LITREF	boolean_t	mvs_save[];

STATICFNDCL void fix_tphold_mvc(char *target, char *srcstart, char *srcend);

error_def(ERR_STACKCRIT);
error_def(ERR_STACKOFLOW);

void flush_jmp (rhdtyp *rtn_base, unsigned char *context, unsigned char *transfer_addr)
{
	mv_stent	*mv_st_ent, *mv_st_prev;
	char		*top;
	unsigned char	*msp_save;
	int4		shift, size, mv_st_type;

	unwind_nocounts();
	/* We are going to mutate the current frame from the program it was running to the program we want it to run.
	 * If the current frame is marked for indr cache cleanup, do that cleanup now and unmark the frame.
	 */
	IF_INDR_FRAME_CLEANUP_CACHE_ENTRY_AND_UNMARK(frame_pointer);

	DBGEHND((stderr, "flush_jmp: Retargetting stack frame 0x"lvaddr" for transfer address 0x"lvaddr"\n", frame_pointer,
		 transfer_addr));
	/* Also unmark the SFF_ETRAP_ERR bit in case it is set. This way we ensure control gets transferred to
	 * the mpc below instead of "error_return" (which is what getframe will do in case the bit is set).
	 * It is ok to clear this bit because the global variable "error_frame" will still be set to point to
	 * this frame so whenever we unwind out of this, we will rethrow the error at the parent frame.
	 */
	assert(!(frame_pointer->flags & SFF_ETRAP_ERR) || (NULL == error_frame) || (error_frame == frame_pointer));
	assert(!(SFT_TRIGR & frame_pointer->type));
	frame_pointer->flags &= SFF_ETRAP_ERR_OFF;	  /* clear SFF_ETRAP_ERR bit */
	frame_pointer->flags &= SFF_IMPLTSTART_CALLD_OFF; /* clear SFF_IMPLTSTART_CALLD bit since this frame is being rewritten */
	GTMTRIG_ONLY(DBGTRIGR((stderr, "flush_jmp: Turrning off SFF_IMPLTSTART_CALLD_OFF in frame 0x"lvaddr"\n", frame_pointer)));
	frame_pointer->rvector = rtn_base;
	frame_pointer->vartab_ptr = (char *)VARTAB_ADR(rtn_base);
	frame_pointer->vartab_len = frame_pointer->rvector->vartab_len;
	frame_pointer->mpc = transfer_addr;
	frame_pointer->ctxt = context;
#ifdef HAS_LITERAL_SECT
	frame_pointer->literal_ptr = (int4 *)LITERAL_ADR(rtn_base);
#endif
	frame_pointer->temp_mvals = frame_pointer->rvector->temp_mvals;
	size = rtn_base->temp_size;
	frame_pointer->temps_ptr = (unsigned char *)frame_pointer - size;
	size += rtn_base->vartab_len * SIZEOF(ht_ent_mname *);
	frame_pointer->l_symtab = (ht_ent_mname **)((char *)frame_pointer - size);
	assert(frame_pointer->type & SFT_COUNT);
	assert((unsigned char *)mv_chain > stacktop && (unsigned char *)mv_chain <= stackbase);
	while (((char *)mv_chain < (char *)frame_pointer) && !mvs_save[mv_chain->mv_st_type])
	{
		assert(MVST_TRIGR != mv_chain->mv_st_type);	/* Should never unwind a trigger frame here */
		msp = (unsigned char *)mv_chain;
		op_oldvar();
	}
	if ((char *)mv_chain > (char *)frame_pointer)
	{
		msp_save = msp;
		msp = (unsigned char *)frame_pointer->l_symtab;
	   	if (msp <= stackwarn)
	   	{
			if (msp <= stacktop)
			{
				msp = msp_save;
				rts_error(VARLSTCNT(1) ERR_STACKOFLOW);
	   		} else
				rts_error(VARLSTCNT(1) ERR_STACKCRIT);
	   	}
		memset(msp, 0, size);
		DBGEHND((stderr, "flush_jmp: Old msp: 0x"lvaddr"  New msp: 0x"lvaddr"\n", msp_save, msp));
		return;
	}
	/* We kept one or more mv_stents for this frame. We may need to shift the stack to get room to create an l_symtab
	 * for this re-purposed frame. In the above loop, we stopped searching the mv_stent chain at the first mv_stent we
	 * knew we had to keep. Since we are moving things around anyway, see if there are any mv_stents associated
	 * with this frame which don't need to be kept and can reclaim.
	 */
	mv_st_ent = mv_chain;
	mv_st_prev = (mv_stent *)((char *)mv_st_ent + mv_st_ent->mv_st_next);
	top = (char *)mv_st_ent + mvs_size[mv_st_ent->mv_st_type];
	while ((char *)mv_st_prev < (char *)frame_pointer)
	{
		mv_st_type = mv_st_prev->mv_st_type;
		assert(MVST_TRIGR != mv_st_type);	/* Should never unwind a trigger frame here */
		if (!mvs_save[mv_st_type])
		{	/* Don't need to keep this mv_stent. Remove it from the chain */
			DBGEHND((stderr, "flush_jmp: Removing no-save mv_stent addr 0x"lvaddr" and type %d\n",
				 mv_st_prev, mv_st_type));
			unw_mv_ent(mv_st_prev);
			mv_st_ent->mv_st_next += mv_st_prev->mv_st_next;
			mv_st_prev = (mv_stent *)((char *)mv_st_prev + mv_st_prev->mv_st_next);
			continue;
		}
		/* We found a previous mv_stent we need to keep. If we had an interveening mv_stent we don't need to
		 * keep, migrate the new keeper mv_stent adjacent to the previous keeper. */
		if (mv_st_prev != (mv_stent *)top)
		{
			DBGEHND((stderr, "flush_jmp: Migrating keeper mv_stent from 0x"lvaddr" to 0x"lvaddr" type %d\n",
				 mv_st_prev, top, mv_st_type));
			if (MVST_TPHOLD == mv_st_type)
			{	/* If we are moving an MVST_TPHOLD mv_stent, find it in the tpstack and fix its
				 * address there too. Else we won't unwind to the correct place on a restart. */
				fix_tphold_mvc(top, (char *)mv_st_prev, ((char *)mv_st_prev + mvs_size[MVST_TPHOLD]));
			}
			memmove(top, mv_st_prev, mvs_size[mv_st_type]);
		}
		DBGEHND((stderr, "flush_jmp: Updating offsets for mv_stent at addr 0x"lvaddr" type %d\n",
			 mv_st_ent, mv_st_ent->mv_st_type));
		mv_st_ent->mv_st_next = mvs_size[mv_st_ent->mv_st_type];
		mv_st_ent = (mv_stent *)top;
		mv_st_ent->mv_st_next += (unsigned int)((char *)mv_st_prev - top);
		top += mvs_size[mv_st_ent->mv_st_type];
		mv_st_prev = (mv_stent *)((char *)mv_st_ent + mv_st_ent->mv_st_next);
	}
	shift = (int4)((char *)frame_pointer - top - size);
	DBGEHND_ONLY(msp_save = msp);
	if (shift)
	{
   		if ((unsigned char *)mv_chain + shift <= stackwarn)
   		{
			if ((unsigned char *)mv_chain + shift <= stacktop)
   				rts_error(VARLSTCNT(1) ERR_STACKOFLOW);
   			else
   				rts_error(VARLSTCNT(1) ERR_STACKCRIT);
      		}
		DBGEHND((stderr, "flush_jmp: Shifting %d bytes of stack from 0x"lvaddr" to 0x"lvaddr" by %d bytes\n",
			 INTCAST(top - (char *)mv_chain), mv_chain, (mv_chain + shift), shift));
		/* Since we are moving one or more mv_stents, it is no more difficult to check the range against the
		 * tp_frame chain than it is to loop through the mv_stents checking each one since the tp_frame stack
		 * is usually no more than 1-3 deep.
		 */
		fix_tphold_mvc(((char *)mv_chain + shift), (char *)mv_chain, ((char *)mv_chain + (top - (char *)mv_chain)));
		memmove((char *)mv_chain + shift, mv_chain, top - (char *)mv_chain);
		mv_chain = (mv_stent *)((char *)mv_chain + shift);
		mv_st_ent = (mv_stent *)((char *)mv_st_ent + shift);
		mv_st_ent->mv_st_next -= shift;
		msp = (unsigned char *)mv_chain;
	}
	memset(frame_pointer->l_symtab, 0, size);
	DBGEHND((stderr, "flush_jmp: Old msp: 0x"lvaddr"  New msp: 0x"lvaddr"\n", msp_save, msp));
	return;
}

/* Routine to fix up the TPHOLD mv_stent address in the tp_stack when flush_jmp shifts the stack */
STATICFNDEF void fix_tphold_mvc(char *target, char *srcstart, char *srcend)
{
	tp_frame	*tf;

	DBGEHND((stderr, "fix_tphold_mvc: entered with target: 0x"lvaddr"  srcstart: 0x"lvaddr"  srcend: 0x"lvaddr"\n",
		 target, srcstart, srcend));
	for (tf = tp_pointer; ((NULL != tf) && ((char *)tf->fp > srcstart)); tf = tf->old_tp_frame)
	{
		if (((char *)tf->mvc >= srcstart) && ((char *)tf->mvc < srcend))
		{
			DBGEHND((stderr, "fix_tphold_mvc: Modifying tp_frame mv_stent value from 0x"lvaddr" to 0x"lvaddr
				 " level %d\n", tf->mvc, ((char *)tf->mvc + (target - srcstart)),
				 tf->mvc->mv_st_cont.mvs_tp_holder.tphold_tlevel));
			tf->mvc = (mv_stent *)((char *)tf->mvc + (target - srcstart));
		}
	}
}
