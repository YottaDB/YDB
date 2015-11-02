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

#include "gtm_string.h"
#include "toktyp.h"
#include <rtnhdr.h>
#include "stack_frame.h"
#include "indir_enum.h"
#include "cmd_qlf.h"
#include "gtm_caseconv.h"
#include "op.h"
#include "do_indir_do.h"
#include "valid_mname.h"

GBLREF	stack_frame		*frame_pointer;
GBLREF	command_qualifier	cmd_qlf;
GBLREF	boolean_t		is_tracing_on;

int do_indir_do(mval *v, unsigned char argcode)
{
	mval		label;
	lnr_tabent	USHBIN_ONLY(*)*addr;
	mident_fixed	ident;
	rhdtyp		*current_rhead;

	if (valid_labname(&v->str))
	{
		memcpy(ident.c, v->str.addr, v->str.len);
		if (!(cmd_qlf.qlf & CQ_LOWER_LABELS))
			lower_to_upper((uchar_ptr_t)ident.c, (uchar_ptr_t)ident.c, v->str.len);
		label.mvtype = MV_STR;
		label.str.len = v->str.len;
		label.str.addr = &ident.c[0];
		addr = op_labaddr(frame_pointer->rvector, &label, 0);
		current_rhead = CURRENT_RHEAD_ADR(frame_pointer->rvector);
		if (argcode == indir_do)
		{	/* If we aren't in an indirect, exfun_frame() is the best way to copy the stack frame as it does not
			 * require re-allocation of the various tables (temps, linkage, literals, etc). But if we are in an
			 * indirect, the various stackframe fields cannot be copied as the indirect has different values so
			 * re-create the frame from the values in the routine header via new_stack_frame().
			 */
			if (!(frame_pointer->flags & SFF_INDCE))
			{
				if (!is_tracing_on)
					exfun_frame();
				else
					exfun_frame_sp();
			} else
			{
				if (!is_tracing_on)
				{
					new_stack_frame(CURRENT_RHEAD_ADR(frame_pointer->rvector),
#							ifdef HAS_LITERAL_SECT
							(unsigned char *)LINKAGE_ADR(current_rhead),
#							else
							PTEXT_ADR(current_rhead),
#							endif
							USHBIN_ONLY(LINE_NUMBER_ADDR(current_rhead, *addr))
							/* On non-shared binary calculate the transfer address to be passed to
							 * new_stack_frame as follows:
							 *  1) get the number stored at addr; this is the offset to the line number
							 *     entry
							 *  2) add the said offset to the address of the routine header; this is the
							 *     address of line number entry
							 *  3) dereference the said address to get the line number of the actual
							 *     program
							 *  4) add the said line number to the address of the routine header
							 */
							NON_USHBIN_ONLY((unsigned char *)((char *)current_rhead
								+ *(int4 *)((char *)current_rhead + *addr))));

				} else
				{
					new_stack_frame_sp(CURRENT_RHEAD_ADR(frame_pointer->rvector),
#							   ifdef HAS_LITERAL_SECT
							   (unsigned char *)LINKAGE_ADR(current_rhead),
#							   else
							   PTEXT_ADR(current_rhead),
#							   endif
							   USHBIN_ONLY(LINE_NUMBER_ADDR(current_rhead, *addr))
							   /* On non-shared binary calculate the transfer address to be passed to
							    * new_stack_frame as follows:
							    *  1) get the number stored at addr; this is the offset to the line
							    *     number entry
							    *  2) add the said offset to the address of the routine header; this is
							    *     the address of line number entry
							    *  3) dereference the said address to get the line number of the actual
							    *     program
							    *  4) add the said line number to the address of the routine header
							    */
							   NON_USHBIN_ONLY((unsigned char *)((char *)current_rhead
								+ *(int4 *)((char *)current_rhead + *addr))));
				}
				return TRUE;
			}
		}
		/* On non-shared binary calculate the mpc pointer similarly to the descriptions above. */
		frame_pointer->mpc =
			USHBIN_ONLY(LINE_NUMBER_ADDR(current_rhead, *addr))
			NON_USHBIN_ONLY((unsigned char *)((char *)current_rhead + *(int4 *)((char *)current_rhead + *addr)));
#		ifdef HAS_LITERAL_SECT
		frame_pointer->ctxt = (unsigned char *)LINKAGE_ADR(current_rhead);
#		else
		frame_pointer->ctxt = PTEXT_ADR(current_rhead);
#		endif
		return TRUE;
	} else
		return FALSE;
}
