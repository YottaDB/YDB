/****************************************************************
 *								*
 *	Copyright 2011, 2013 Fidelity Information Services, Inc	*
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
#include "op.h"
#include "flush_jmp.h"
#include "dollar_zlevel.h"
#include "golevel.h"
#include <auto_zlink.h>
#include "error.h"
#include "gtmimagename.h"
#ifdef UNIX
#include "gtm_unlink_all.h"
#endif
#ifdef GTM_TRIGGER
#include "gtm_trigger_trc.h"
#endif
#ifdef VMS
#include "pdscdef.h"
#include "proc_desc.h"
#endif

GBLREF	stack_frame	*frame_pointer;
#ifdef GTM_TRIGGER
GBLREF	boolean_t	goframes_unwound_trigger;
#endif

LITREF	gtmImageName 	gtmImageNames[];

error_def(ERR_LABELUNKNOWN);
error_def(ERR_RTNNAME);
error_def(ERR_ZGOCALLOUTIN);
error_def(ERR_ZGOTOINVLVL);
error_def(ERR_ZGOTOINVLVL2);
error_def(ERR_ZGOTOLTZERO);
error_def(ERR_ZGOTOTOOBIG);

void op_zgoto(mval *rtn_name, mval *lbl_name, int offset, int level)
{
	stack_frame	*fp, *fpprev, *base_frame;
	int4		curlvl;
	mval		rtnname, lblname;
	rhdtyp		*rtnhdr;
	lnr_tabent 	USHBIN_ONLY(*)*lnrptr;
	char		rtnname_buff[SIZEOF(mident_fixed)], lblname_buff[SIZEOF(mident_fixed)];
	DEBUG_ONLY(int4	dlevel;)

	/* Validate level parm */
	curlvl = dollar_zlevel();
        if (0 > level)
	{	/* Negative level specified, means to use relative level change */
		if ((-level) > curlvl)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZGOTOLTZERO);
		level += curlvl;	/* Compute relative desired level */
	} else
	{	/* Else level is the level we wish to achieve - compute unrolls necessary */
		if (0 > (curlvl - level))
			/* Couldn't get to the level we were trying to unwind to */
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZGOTOTOOBIG);
	}
	/* Migrate mval parm contents to private buffers since the mvals could die as we unwind things */
	MV_FORCE_STR(rtn_name);
	MV_FORCE_STR(lbl_name);
	rtnname = *rtn_name;
	lblname = *lbl_name;
	memcpy(rtnname_buff, rtnname.str.addr, rtnname.str.len);
	rtnname.str.addr = rtnname_buff;
	memcpy(lblname_buff, lblname.str.addr, lblname.str.len);
	lblname.str.addr = lblname_buff;
	DBGEHND((stdout, "op_zgoto: rtnname: %.*s  lblname: %.*s  offset: %d  level: %d\n",
		 rtnname.str.len, rtnname.str.addr, lblname.str.len, lblname.str.addr, offset, level));
	/* Validate entryref before do any unwinding */
	if (0 == rtnname.str.len)
	{	/* If no routine name given, take it from the currently running routine unless the label name
		 * is also NULL. In that case, we must be doing an indirect routine name only and the supplied name
		 * was NULL.
		 */
		if (0 == lblname.str.len)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_RTNNAME);
		rtnhdr = frame_pointer->rvector;
		if (0 == level)
		{	/* If doing unlink, recall name of routine as well as will need it later */
			memcpy(rtnname_buff, rtnhdr->routine_name.addr, rtnhdr->routine_name.len);
			rtnname.str.addr = rtnname_buff;
			rtnname.str.len = rtnhdr->routine_name.len;
		}
	} else
	{
		rtnhdr = op_rhdaddr(&rtnname, NULL);	/* Does an autozlink if necessary */
#		ifdef VMS
		if (PDSC_FLAGS == ((proc_desc *)rtnhdr)->flags) /* it's a procedure descriptor, not a routine header */
		{
			rtnhdr = (rhdtyp *)(((proc_desc *)rtnhdr)->code_address);
			/* Make sure this if-test is sufficient to distinguish between procedure descriptors and routine headers */
			assert(PDSC_FLAGS != ((proc_desc *)rtnhdr)->flags);
		}
#		endif
	}
	assert(NULL != rtnhdr);
	lnrptr = op_labaddr(rtnhdr, &lblname, offset);
	assert(NULL != lnrptr);
#	ifdef VMS
	/* VMS does not support unlink so any level 0 request that passes earlier checks just generates an error
	 * since the base frame cannot be rewritten as a GTM frame.
	 */
	if (0 == level)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZGOTOINVLVL2);
#	endif
#	ifdef GTM_TRIGGER
	if (!IS_GTM_IMAGE && (1 >= level))
		/* In MUPIP (or other utility that gets trigger support in the future), levels 0 and 1 refer to
		 * the pseudo baseframe and initial stack levels which are not rewritable (in the case where an
		 * entry ref was coded) and are not resume-able (if no entry ref were specified) so we cannot
		 * permit ZGOTOs to these levels in a utility.
		 */
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_ZGOTOINVLVL, 3, GTMIMAGENAMETXT(image_type), level);
#	endif
#	ifdef UNIX
	/* One last check if we are unlinking, make sure no call-in frames exist on our stack */
	if (0 == level)
	{
		for (fp = frame_pointer; NULL != fp; fp = fpprev)
		{
			fpprev = fp->old_frame_pointer;
			if (!(fp->type & SFT_COUNT))
				continue;
			if (fp->flags & SFF_CI)
				/* We have a call-in frame - cannot do unlink */
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZGOCALLOUTIN);
			if (NULL == fpprev)
			{	/* Next frame is some sort of base frame */
#				ifdef GTM_TRIGGER
				if (fp->type & SFT_TRIGR)
				{	/* Have a trigger baseframe, pick up stack continuation frame_pointer stored by
					 *base_frame() */
					fpprev = *(stack_frame **)(fp + 1);
					continue;
				} else
#				endif
					break;			/* Some other base frame that stops us */
			}
		}
		/* Full unlink/unwind requested. First unlink everything, then relink our target entryref */
		gtm_unlink_all();
		/* Now that everything is unwound except the frame we will rewrite when we start going forward again, we need to
		 * find the base frame. This frame contains the same rvector pointer that the level 1 stack frame routine has so
		 * it needs to also be rewritten once we link our new "1st routine". Find the base frame we will be modifying.
		 */
		for (base_frame = frame_pointer; ((NULL != base_frame) && (NULL != base_frame->old_frame_pointer));
		     base_frame = base_frame->old_frame_pointer);
		assert(NULL != base_frame);
		rtnhdr = op_rhdaddr(&rtnname, NULL);
		assert(NULL != rtnhdr);
		base_frame->rvector = rtnhdr;
		lnrptr = op_labaddr(rtnhdr, &lblname, offset);
		assert(NULL != lnrptr);
	} else
#	endif	/* UNIX */
	{	/* Unwind to our target level (UNIX if 0 != level  and VMS [all]) */
		GOLEVEL(level, TRUE);	/* Unwind the trigger base frame if we run into one */
		assert(level == dollar_zlevel());
	}
	/* Convert current stack frame to the frame for the entry ref we want to branch to */
	USHBIN_ONLY(flush_jmp(rtnhdr, (unsigned char *)LINKAGE_ADR(rtnhdr), LINE_NUMBER_ADDR(rtnhdr, *lnrptr)));
	/* on non-shared binary calculate the transfer address to be passed to flush_jmp as follows:
	 * 	1) get the number stored at lnrptr; this is the offset to the line number entry
	 *	2) add the said offset to the address of the routine header; this is the address of line number entry
	 *	3) dereference the said address to get the line number of the actual program
	 *	4) add the said line number to the address of the routine header
	 */
	NON_USHBIN_ONLY(flush_jmp(rtnhdr, (unsigned char *)LINKAGE_ADR(rtnhdr),
		(unsigned char *)((int)rtnhdr + *(int *)((int)rtnhdr + *lnrptr))));
	DBGEHND((stderr, "op_zgoto: Resuming at frame 0x"lvaddr" with type 0x%04lx\n", frame_pointer, frame_pointer->type));
#	ifdef GTM_TRIGGER
	if (goframes_unwound_trigger)
	{	/* If goframes() called by golevel unwound a trigger base frame, we must use MUM_TSTART to unroll the
		 * C stack before invoking the return frame. Otherwise we can just return and avoid the overhead that
		 * MUM_TSTART incurs.
		 */
		MUM_TSTART;
	}
#	endif
}
