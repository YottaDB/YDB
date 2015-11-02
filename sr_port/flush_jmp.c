/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_string.h"

#include "hashtab_mname.h"	/* needed for lv_val.h */
#include "lv_val.h"
#include "sbs_blk.h"
#include "rtnhdr.h"
#include "mv_stent.h"
#include "objlabel.h"
#include "cache.h"
#include "stack_frame.h"
#include "cache_cleanup.h"
#include "op.h"
#include "unwind_nocounts.h"
#include "flush_jmp.h"

GBLREF	mv_stent	*mv_chain;
GBLREF	unsigned char	*stackbase,*stacktop,*msp,*stackwarn;
GBLREF	stack_frame	*frame_pointer;
GBLREF	stack_frame	*error_frame;
GBLREF	symval		*curr_symval;

/* All mv_stent types that need to be preserved are indicated by the mvs_save[] array.
 * MVST_STCK_SP (which is the same as the MVST_STCK type everywhere else is handled specially here.
 * This entry is created by mdb_condition_handler to stack the "stackwarn" global variable.
 * This one needs to be preserved since our encountering this type in flush_jmp.c indicates that we are currently
 * in the error-handler of a STACKCRIT error which in turn has "GOTO ..." in the $ZTRAP/$ETRAP that is
 * causing us to mutate the current frame we are executing in with the contents pointed to by the GOTO.
 * In that case, we do not want to restore "stackwarn" to its previous value since we are still handling
 * the STACKCRIT error. So we set MVST_STCK_SP type as needing to be preserved.
 */
LITDEF	boolean_t	mvs_save[] = {
	TRUE,	/* MVST_MSAV */
	FALSE,	/* MVST_MVAL */
	TRUE,	/* MVST_STAB */
	FALSE,	/* MVST_IARR */
	TRUE,	/* MVST_NTAB */
	TRUE,	/* MVST_PARM */
	TRUE,	/* MVST_PVAL */
	FALSE,	/* MVST_STCK */
	TRUE,	/* MVST_NVAL */
	TRUE,	/* MVST_TVAL */
	TRUE,	/* MVST_TPHOLD */
	TRUE,	/* MVST_ZINTR */
	FALSE,	/* MVST_ZINTDEV */
	TRUE	/* MVST_STCK_SP */
};

void flush_jmp (rhdtyp *rtn_base, unsigned char *context, unsigned char *transfer_addr)
{

	mv_stent	*mv_st_ent, *mv_st_prev;
	char		*top;
	unsigned char	*msp_save;
	int4		shift, size;
	int4		mv_st_type;

	error_def(ERR_STACKOFLOW);
	error_def(ERR_STACKCRIT);

	unwind_nocounts();
	/* We are going to mutate the current frame from the program it was running to the program we want it to run.
	 * If the current frame is marked for indr cache cleanup, do that cleanup now and unmark the frame.
	 */
	IF_INDR_FRAME_CLEANUP_CACHE_ENTRY_AND_UNMARK(frame_pointer);

	/* Also unmark the SFF_ETRAP_ERR bit in case it is set. This way we ensure control gets transferred to
	 * the mpc below instead of "error_return" (which is what getframe will do in case the bit is set).
	 * It is ok to clear this bit because the global variable "error_frame" will still be set to point to
	 * this frame so whenever we unwind out of this, we will rethrow the error at the parent frame.
	 */
	assert(!(frame_pointer->flags & SFF_ETRAP_ERR) || (NULL == error_frame) || (error_frame == frame_pointer));
	if (frame_pointer->flags & SFF_ETRAP_ERR)
		frame_pointer->flags &= SFF_ETRAP_ERR_OFF;	/* clear the SFF_ETRAP_ERR bit */
	frame_pointer->rvector = rtn_base;
	frame_pointer->vartab_ptr = (char *)VARTAB_ADR(rtn_base);
	frame_pointer->vartab_len = frame_pointer->rvector->vartab_len;
	frame_pointer->mpc = transfer_addr;
	frame_pointer->ctxt = context;
#ifdef HAS_LITERAL_SECT
	frame_pointer->literal_ptr = (int4 *)0;
#endif
	frame_pointer->temp_mvals = frame_pointer->rvector->temp_mvals;
	size = rtn_base->temp_size;
	frame_pointer->temps_ptr = (unsigned char *) frame_pointer - size;
	size += rtn_base->vartab_len * sizeof(mval *);
	frame_pointer->l_symtab = (mval **)((char *) frame_pointer - size);
	assert(frame_pointer->type & SFT_COUNT);
	assert((unsigned char *) mv_chain > stacktop && (unsigned char *) mv_chain <= stackbase);

	while ((char *) mv_chain < (char *) frame_pointer && !mvs_save[mv_chain->mv_st_type])
	{
		msp = (unsigned char *)mv_chain;
		op_oldvar();
	}
	if ((char *) mv_chain > (char *) frame_pointer)
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
		return;
	}
	mv_st_ent = mv_chain;
	mv_st_prev = (mv_stent *)((char *) mv_st_ent + mv_st_ent->mv_st_next);
	top = (char *) mv_st_ent + mvs_size[mv_st_ent->mv_st_type];
	while ((char *) mv_st_prev < (char *) frame_pointer)
	{
		mv_st_type = mv_st_prev->mv_st_type;
		if (!mvs_save[mv_st_type])
		{
			unw_mv_ent(mv_st_prev);
			mv_st_ent->mv_st_next += mv_st_prev->mv_st_next;
			mv_st_prev = (mv_stent *)((char *) mv_st_prev + mv_st_prev->mv_st_next);
			continue;
		}
		if (mv_st_prev != (mv_stent *)top)
			memmove(top, mv_st_prev, mvs_size[mv_st_prev->mv_st_type]);
		mv_st_ent->mv_st_next = mvs_size[mv_st_ent->mv_st_type];
		mv_st_ent = (mv_stent *)top;
		mv_st_ent->mv_st_next += (char *) mv_st_prev - top;
		top += mvs_size[mv_st_ent->mv_st_type];
		mv_st_prev = (mv_stent *)((char *) mv_st_ent + mv_st_ent->mv_st_next);
	}
	shift = (int4)((char *) frame_pointer - top - size);
	if (shift)
	{
   		if ((unsigned char *)mv_chain + shift <= stackwarn)
   		{
			if ((unsigned char *)mv_chain + shift <= stacktop)
   				rts_error(VARLSTCNT(1) ERR_STACKOFLOW);
   			else
   				rts_error(VARLSTCNT(1) ERR_STACKCRIT);
      		}
		memmove((char *) mv_chain + shift, mv_chain, top - (char *) mv_chain);
		mv_chain = (mv_stent *)((char *) mv_chain + shift);
		mv_st_ent = (mv_stent *)((char *) mv_st_ent + shift);
		mv_st_ent->mv_st_next -= shift;
		msp = (unsigned char *) mv_chain;
	}
	memset(frame_pointer->l_symtab, 0, size);
	return;
}
