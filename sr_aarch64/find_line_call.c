/****************************************************************
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 * Copyright (c) 2018 Stephen L Johnson. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

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

#include "mdef.h"

#include "aarch64_registers.h"
#include "aarch64_gtm_registers.h"
#include "aarch64.h"
#include "xfer_enum.h"
#include <rtnhdr.h>	/* Needed by zbreak.h */
#include "zbreak.h"

/* Numeric literals are pushed on the stack or loaded into argument registers with a combination of the following instructions: */
#define	PUSH_NUM_LIT1	(AARCH64_INS_MOV_IMM & (AARCH64_MASK_OP_12 << AARCH64_SHIFT_OP_20))
#define PUSH_NUM_LIT2	(AARCH64_INS_MOVK    & (AARCH64_MASK_OP_12 << AARCH64_SHIFT_OP_20))
#define PUSH_NUM_LIT3	(AARCH64_INS_MOV_REG & (AARCH64_MASK_OP_12 << AARCH64_SHIFT_OP_20))
#define PUSH_NUM_LIT4	(AARCH64_INS_ADD_IMM & (AARCH64_MASK_OP_8  << AARCH64_SHIFT_OP_24))
#define PUSH_NUM_LIT5	(AARCH64_INS_STR_X   & (AARCH64_MASK_OP_12 << AARCH64_SHIFT_OP_20))
#define PUSH_NUM_LIT6	(AARCH64_INS_SUB_REG & (AARCH64_MASK_OP_8  << AARCH64_SHIFT_OP_24))
#define PUSH_NUM_LIT7	(AARCH64_INS_LSR     & (AARCH64_MASK_OP_12 << AARCH64_SHIFT_OP_20))

/* The first instruction sets up a procedure descriptor and transfer address; the second makes the call.
 * 	ldr	x15, [x23, offset]
 *	blr	x15
 */
#define	LOAD_XFER_ADDR	(AARCH64_INS_LDR_X											\
				| (GTM_REG_CODEGEN_TEMP << AARCH64_SHIFT_RT)							\
				| (GTM_REG_XFER_TABLE << AARCH64_SHIFT_RN))
#define	XFER_CALL	(AARCH64_INS_BLR											\
				| GTM_REG_CODEGEN_TEMP << AARCH64_SHIFT_RN)

zb_code  *find_line_call(void *addr)
{
	uint4	*call_addr;
	uint4	*xfer_addr;
	short	xfer_offset;

	call_addr = addr;
	if ((LOAD_XFER_ADDR == (*call_addr & ~(AARCH64_MASK_IMM12 << AARCH64_SHIFT_IMM12))) && (XFER_CALL == *(call_addr + 1)))
	{
		/* It's a transfer table load. */
		xfer_addr = call_addr;
		xfer_offset = (*xfer_addr >> AARCH64_SHIFT_IMM12) & AARCH64_MASK_IMM12;
		call_addr++;
		assert(XFER_CALL == *call_addr);
		return ((zb_code *)addr);
	}
	/* Locate and skip over series of operand pushes.  This is not rigorous, but should catch a series of
	 * numeric constant arguments:
	 */
	xfer_addr = call_addr;
	if (   (PUSH_NUM_LIT1 == (*call_addr & (AARCH64_MASK_OP_12 << AARCH64_SHIFT_OP_20)))
	    || (PUSH_NUM_LIT2 == (*call_addr & (AARCH64_MASK_OP_12 << AARCH64_SHIFT_OP_20)))
	    || (PUSH_NUM_LIT3 == (*call_addr & (AARCH64_MASK_OP_12 << AARCH64_SHIFT_OP_20)))
	    || (PUSH_NUM_LIT4 == (*call_addr & (AARCH64_MASK_OP_8  << AARCH64_SHIFT_OP_24)))
	    || (PUSH_NUM_LIT5 == (*call_addr & (AARCH64_MASK_OP_12 << AARCH64_SHIFT_OP_20)))
	    || (PUSH_NUM_LIT6 == (*call_addr & (AARCH64_MASK_OP_8  << AARCH64_SHIFT_OP_24)))
	    || (PUSH_NUM_LIT7 == (*call_addr & (AARCH64_MASK_OP_12 << AARCH64_SHIFT_OP_20))))
	{
		
		while (    (PUSH_NUM_LIT1 == (*call_addr & (AARCH64_MASK_OP_12 << AARCH64_SHIFT_OP_20)))
			|| (PUSH_NUM_LIT2 == (*call_addr & (AARCH64_MASK_OP_12 << AARCH64_SHIFT_OP_20)))
			|| (PUSH_NUM_LIT3 == (*call_addr & (AARCH64_MASK_OP_12 << AARCH64_SHIFT_OP_20)))
			|| (PUSH_NUM_LIT4 == (*call_addr & (AARCH64_MASK_OP_8  << AARCH64_SHIFT_OP_24)))
			|| (PUSH_NUM_LIT5 == (*call_addr & (AARCH64_MASK_OP_12 << AARCH64_SHIFT_OP_20)))
			|| (PUSH_NUM_LIT6 == (*call_addr & (AARCH64_MASK_OP_8  << AARCH64_SHIFT_OP_24)))
			|| (PUSH_NUM_LIT7 == (*call_addr & (AARCH64_MASK_OP_12 << AARCH64_SHIFT_OP_20))))
		{
			call_addr++;
		}
		/* If it's not a transfer table call, give up.  */
		if ((LOAD_XFER_ADDR != (*call_addr & ~(AARCH64_MASK_IMM12 << AARCH64_SHIFT_IMM12)))
			|| (XFER_CALL != *(call_addr + 1)))
		{
			return ((zb_code *)addr);
		}
		xfer_addr = call_addr;
		xfer_offset = (*xfer_addr >> AARCH64_SHIFT_IMM12) & AARCH64_MASK_IMM12;
		if ((xf_linefetch != xfer_offset) && (xf_zbfetch != xfer_offset))
		{
			return ((zb_code *)addr);
		}
	} else if ((LOAD_XFER_ADDR == (*call_addr & ~(AARCH64_MASK_IMM12 << AARCH64_SHIFT_IMM12)))
			&& (XFER_CALL == *(call_addr + 1)))
	{
		xfer_addr = call_addr;
		xfer_offset = (*xfer_addr >> AARCH64_SHIFT_IMM12) & AARCH64_MASK_IMM12;
		if ((xf_linestart != xfer_offset) && (xf_zbstart != xfer_offset))
		{
			return ((zb_code *)addr);
		}
	}
	return ((zb_code *)xfer_addr);

}
