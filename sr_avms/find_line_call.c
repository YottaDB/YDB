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

#include "axp_registers.h"
#include "axp_gtm_registers.h"
#include "axp.h"
#include "xfer_enum.h"
#include <rtnhdr.h>	/* Needed by zbreak.h */
#include "zbreak.h"

/* Numeric literals are pushed on the stack or loaded into argument registers with a combination of the following instructions: */
#define	PUSH_NUM_LIT1	(ALPHA_INS_LDAH)
#define PUSH_NUM_LIT2	(ALPHA_INS_LDA)
#define PUSH_NUM_LIT3	(ALPHA_INS_STQ	| (GTM_REG_ACCUM << ALPHA_SHIFT_RA)	| (ALPHA_REG_SP << ALPHA_SHIFT_RB))

/* The first two instructions set up a procedure descriptor and transfer address; the third makes the call.  */
#define	LOAD_XFER_ADDR1	(ALPHA_INS_LDL	| (ALPHA_REG_PV << ALPHA_SHIFT_RA)	| (GTM_REG_XFER_TABLE << ALPHA_SHIFT_RB))
#define LOAD_XFER_ADDR2	(ALPHA_INS_LDQ	| (ALPHA_REG_RA << ALPHA_SHIFT_RA)	| (ALPHA_REG_PV << ALPHA_SHIFT_RB) \
					| ((8 & ALPHA_MASK_DISP) << ALPHA_SHIFT_DISP))
#define	XFER_CALL	(ALPHA_INS_JSR	| (ALPHA_REG_RA << ALPHA_SHIFT_RA)	| (ALPHA_REG_RA << ALPHA_SHIFT_RB))


/*	find_line_call searches through machine instructions starting at the address corresponding to the
 *	beginning of a MUMPS statement looking for a call to any of op_linestart, op_zbstart, op_linefetch,
 *	or op_zbfetch.  It will return the address of the displacement field of the instruction that indexes
 *	to the desired runtime routine in order that its caller may examine or change that field.
 *
 *	find_line_call will skip over any number of numeric literal arguments to op_linefetch or op_zbfetch
 *	in its search for one of the desired calls.  If it doesn't find any, it will return the address of
 *	the offset field of the instruction to which addr points.
 *
 *	entry
 *		addr	address of beginning of MUMPS statement
 *
 *	exit
 *		returns address of displacement field containing offset into the transfer table in the
 *			instruction that refers to the transfer table
 */

zb_code	*find_line_call(void *addr)
{
	uint4	*call_addr;
	uint4	*xfer_addr;
	short	xfer_offset;

	call_addr = addr;
	if ((LOAD_XFER_ADDR1 == (*call_addr & ~(ALPHA_MASK_DISP << ALPHA_SHIFT_DISP))) && (LOAD_XFER_ADDR2 == *(call_addr + 1)))
	{
		/* It's a transfer table load. */
		xfer_addr = call_addr;
		xfer_offset = (*xfer_addr >> ALPHA_SHIFT_DISP) & ALPHA_MASK_DISP;
		call_addr += 2;
		assert(XFER_CALL == *call_addr);
		return ((zb_code *)addr);
	}
	/* Locate and skip over series of operand pushes.  This is not rigorous, but should catch a series of
	 * numeric constant arguments:
	 */
	xfer_addr = call_addr;
	if (   (PUSH_NUM_LIT1 == (*call_addr &  (ALPHA_MASK_OP << ALPHA_SHIFT_OP)))
	    || (PUSH_NUM_LIT2 == (*call_addr &  (ALPHA_MASK_OP << ALPHA_SHIFT_OP)))
            || (PUSH_NUM_LIT3 == (*call_addr & ~(ALPHA_MASK_DISP << ALPHA_SHIFT_DISP))))
	{
		while (    (PUSH_NUM_LIT1 == (*call_addr &  (ALPHA_MASK_OP << ALPHA_SHIFT_OP)))
			|| (PUSH_NUM_LIT2 == (*call_addr &  (ALPHA_MASK_OP << ALPHA_SHIFT_OP)))
			|| (PUSH_NUM_LIT3 == (*call_addr & ~(ALPHA_MASK_DISP << ALPHA_SHIFT_DISP))))
			call_addr++;
		/* If it's not a transfer table call, give up.  */
		if (   (LOAD_XFER_ADDR1	!= (*call_addr & ~(ALPHA_MASK_DISP << ALPHA_SHIFT_DISP)))
		    || (LOAD_XFER_ADDR2	!= *(call_addr + 1))
		    || (XFER_CALL 	!= *(call_addr + 2)))
			return ((zb_code *)addr);
		xfer_addr = call_addr;
		xfer_offset = (*call_addr >> ALPHA_SHIFT_DISP) & ALPHA_MASK_DISP;
		call_addr += 3;
		if ((xf_linefetch * SIZEOF(int4) != xfer_offset) && (xf_zbfetch * SIZEOF(int4) != xfer_offset))
			return ((zb_code *)addr);
	} else if ((LOAD_XFER_ADDR1	== (*call_addr & ~(ALPHA_MASK_DISP << ALPHA_SHIFT_DISP)))
		&& (LOAD_XFER_ADDR2	== *(call_addr + 1))
		&& (XFER_CALL		== *(call_addr + 2)))
	{
		xfer_addr = call_addr;
		xfer_offset = (*call_addr >> ALPHA_SHIFT_DISP) & ALPHA_MASK_DISP;
		if ((xf_linestart * SIZEOF(int4) != xfer_offset) && (xf_zbstart * SIZEOF(int4) != xfer_offset))
			return ((zb_code *)addr);
	}
	return ((zb_code *)xfer_addr);
}
