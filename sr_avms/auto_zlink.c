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

#include "axp_registers.h"
#include "axp_gtm_registers.h"
#include "axp.h"
#include "urx.h"
#include <rtnhdr.h>
#include "stack_frame.h"
#include "op.h"
#include <auto_zlink.h>

#define INST_SZ	1

#define LDQ_R17_OFF	((-5)*INST_SZ)
#define LDQ_R16_OFF	((-4)*INST_SZ)
#define LDL_R27_OFF	((-3)*INST_SZ)
#define LDQ_R26_OFF	((-2)*INST_SZ)
#define JMP_R26_OFF	((-1)*INST_SZ)

#define LDQ_R17_X2_R13	(ALPHA_INS_LDQ  |  (ALPHA_REG_A1 << ALPHA_SHIFT_RA)  |  (GTM_REG_PV << ALPHA_SHIFT_RB))
#define LDQ_R16_X1_R13	(ALPHA_INS_LDQ  |  (ALPHA_REG_A0 << ALPHA_SHIFT_RA)  |  (GTM_REG_PV << ALPHA_SHIFT_RB))
#define LDL_R27_XF_R11	(ALPHA_INS_LDL  |  (ALPHA_REG_PV << ALPHA_SHIFT_RA)  |  (GTM_REG_XFER_TABLE << ALPHA_SHIFT_RB))
#define LDQ_R26_8_R27	(ALPHA_INS_LDQ  |  (ALPHA_REG_RA << ALPHA_SHIFT_RA)  |  (ALPHA_REG_PV << ALPHA_SHIFT_RB)  \
					|  (8 << ALPHA_SHIFT_DISP))
#define JMP_R26_R26	(ALPHA_INS_JSR  |  (ALPHA_REG_RA << ALPHA_SHIFT_RA)  |  (ALPHA_REG_RA << ALPHA_SHIFT_RB))

GBLREF stack_frame	*frame_pointer;

error_def(ERR_LABELUNKNOWN);
error_def(ERR_ROUTINEUNKNOWN);

rhdtyp *auto_zlink(uint4 *pc, uint4 *line)
{
	uint4		*A_proc_desc, *A_labaddr;
	mstr		rname;
	mval		rtn;
	rhdtyp		*rhead;
	urx_rtnref	*rtnurx;
	mident_fixed	rname_local;

/* ASSUMPTION	The instructions immediately preceding the current mpc form a transfer table call.
 *		There will be two arguments to this call:
 *			the address of a procedure descriptor and
 *			the address of a pointer to a value which is the offset from the beginning of the routine header to
 *				the address of the first instruction to be executed in the routine to be called, as specified
 *				by the [label][+offset] field (or the beginning of the routine if label and offset are not
 *				specified
 *
 *		The entire instruction sequence will look like this:
 *			ldq	r17, x2(r13)
 *			ldq	r16, x1(r13)
 *			ldl	r27, 4*xf_off(r11)
 *			ldq	r26, 8(r27)
 *			jmp	r26, (r26)
 *
 *		N.B., the instruction sequence above occurs in compiled object modules; in direct mode, the argument
 *		registers are loaded via "ldl" instructions.  However, this routine should never be invoked from direct
 *		mode because the applicable routine should have been ZLINK'ed by prior calls to op_labaddr and op_rhdaddr.
 */

	/* Verify calling sequence. */
	if (   (*(pc + LDQ_R17_OFF) & ~(ALPHA_MASK_DISP << ALPHA_SHIFT_DISP)) != LDQ_R17_X2_R13
	    || (*(pc + LDQ_R16_OFF) & ~(ALPHA_MASK_DISP << ALPHA_SHIFT_DISP)) != LDQ_R16_X1_R13
	    || (*(pc + LDL_R27_OFF) & ~(ALPHA_MASK_DISP << ALPHA_SHIFT_DISP)) != LDL_R27_XF_R11
	    || (*(pc + LDQ_R26_OFF) != LDQ_R26_8_R27)
	    || (*(pc + JMP_R26_OFF) != JMP_R26_R26)							)
		GTMASSERT;
	/* Calling sequence O.K.; get address(address(procedure descriptor)) and address(address(label offset)).  */
	A_proc_desc = (*(pc + LDQ_R16_OFF) & (ALPHA_MASK_DISP << ALPHA_SHIFT_DISP)) + frame_pointer->ctxt;
	A_labaddr   = (*(pc + LDQ_R17_OFF) & (ALPHA_MASK_DISP << ALPHA_SHIFT_DISP)) + frame_pointer->ctxt;
	if (azl_geturxrtn(A_proc_desc, &rname, &rtnurx))
	{
		assert(rname.len <= MAX_MIDENT_LEN);
		assert(0 != rname.addr);

		/* Copy rname into local storage because azl_geturxrtn sets rname.addr to an address that is
		 * free'd during op_zlink and before the call to find_rtn_hdr.
		 */
                memcpy(rname_local.c, rname.addr, rname.len);
                rname.addr = rname_local.c;
		assert(0 != rtnurx);
		assert(azl_geturxlab(A_labaddr, rtnurx));
		assert(0 == find_rtn_hdr(&rname));
		rtn.mvtype = MV_STR;
		rtn.str.len = rname.len;
		rtn.str.addr = rname.addr;
		op_zlink(&rtn, 0);
		if (0 != (rhead = find_rtn_hdr(&rname)))
		{
			*line = *A_labaddr;
			if (0 == *line)
				rts_error(VARLSTCNT(1) ERR_LABELUNKNOWN);
			return rhead;
		}
	}
	rts_error(VARLSTCNT(1) ERR_ROUTINEUNKNOWN);
}
