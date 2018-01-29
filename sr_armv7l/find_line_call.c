/****************************************************************
 *								*
 * Copyright (c) 2017-2018 YottaDB LLC. and/or its subsidiaries.*
 * All rights reserved.						*
 *								*
 * Copyright (c) 2017-2018 Stephen L Johnson.			*
 * All rights reserved.						*
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

#include "arm_registers.h"
#include "arm_gtm_registers.h"
#include "arm.h"
#include "xfer_enum.h"
#include <rtnhdr.h>	/* Needed by zbreak.h */
#include "zbreak.h"

/* Numeric literals are pushed on the stack or loaded into argument registers with a combination of the following instructions: */
#define	PUSH_NUM_LIT1	(ARM_INS_MOV_IMM & (ARM_MASK_OP << ARM_SHIFT_OP))
#ifdef __armv7l__
#define PUSH_NUM_LIT2	(ARM_INS_MOVW & (ARM_MASK_OP << ARM_SHIFT_OP))
#else	/* __armv6l__ */
#define PUSH_NUM_LIT2	((ARM_INS_LDR & (ARM_MASK_OP << ARM_SHIFT_OP)) | (ARM_REG_PC << ARM_SHIFT_RN))
#endif
#define PUSH_NUM_LIT3	(ARM_INS_MOV_REG & (ARM_MASK_OP << ARM_SHIFT_OP))
#define PUSH_NUM_LIT4	(ARM_INS_ADD_IMM & (ARM_MASK_OP << ARM_SHIFT_OP))
#define PUSH_NUM_LIT5	((ARM_INS_STR | ARM_U_BIT_ON) & (ARM_MASK_OP << ARM_SHIFT_OP))
#define PUSH_NUM_LIT6	(ARM_INS_SUB_REG & (ARM_MASK_OP << ARM_SHIFT_OP))
#define PUSH_NUM_LIT7	(ARM_INS_LSR & (ARM_MASK_OP << ARM_SHIFT_OP))

/* The first instruction sets up a procedure descriptor and transfer address; the second makes the call.  */
#define	LOAD_XFER_ADDR	(ARM_INS_LDR												\
				| ARM_U_BIT_ON											\
				| (GTM_REG_CODEGEN_TEMP << ARM_SHIFT_RT)							\
				| (GTM_REG_XFER_TABLE << ARM_SHIFT_RN))
#define	XFER_CALL	(ARM_INS_BLX												\
				| GTM_REG_CODEGEN_TEMP << ARM_SHIFT_RM)

zb_code  *find_line_call(void *addr)
{
	uint4	*call_addr;
	uint4	*xfer_addr;
	short	xfer_offset;

	call_addr = addr;
	if ((LOAD_XFER_ADDR == (*call_addr & ~(ARM_MASK_IMM12 << ARM_SHIFT_IMM12))) && (XFER_CALL == *(call_addr + 1)))
	{
		/* It's a transfer table load. */
		xfer_addr = call_addr;
		xfer_offset = (*xfer_addr >> ARM_SHIFT_IMM12) & ARM_MASK_IMM12;
		call_addr++;
		assert(XFER_CALL == *call_addr);
		return ((zb_code *)addr);
	}
	/* Locate and skip over series of operand pushes.  This is not rigorous, but should catch a series of
	 * numeric constant arguments:
	 */
	xfer_addr = call_addr;
	if (   (PUSH_NUM_LIT1 == (*call_addr &  (ARM_MASK_OP << ARM_SHIFT_OP)))
	    || (PUSH_NUM_LIT2 == (*call_addr & ((ARM_MASK_OP << ARM_SHIFT_OP) | (ARM_MASK_REG << ARM_SHIFT_RN))))
	    || (PUSH_NUM_LIT3 == (*call_addr &  (ARM_MASK_OP << ARM_SHIFT_OP)))
	    || (PUSH_NUM_LIT4 == (*call_addr &  (ARM_MASK_OP << ARM_SHIFT_OP)))
	    || (PUSH_NUM_LIT5 == (*call_addr &  (ARM_MASK_OP << ARM_SHIFT_OP)))
	    || (PUSH_NUM_LIT6 == (*call_addr &  (ARM_MASK_OP << ARM_SHIFT_OP)))
	    || (PUSH_NUM_LIT7 == (*call_addr &  (ARM_MASK_OP << ARM_SHIFT_OP))))
	{
		while (    (PUSH_NUM_LIT1 == (*call_addr &  (ARM_MASK_OP << ARM_SHIFT_OP)))
			|| (PUSH_NUM_LIT2 == (*call_addr & ((ARM_MASK_OP << ARM_SHIFT_OP) | (ARM_MASK_REG << ARM_SHIFT_RN))))
			|| (PUSH_NUM_LIT3 == (*call_addr &  (ARM_MASK_OP << ARM_SHIFT_OP)))
			|| (PUSH_NUM_LIT4 == (*call_addr &  (ARM_MASK_OP << ARM_SHIFT_OP)))
			|| (PUSH_NUM_LIT5 == (*call_addr &  (ARM_MASK_OP << ARM_SHIFT_OP)))
			|| (PUSH_NUM_LIT6 == (*call_addr &  (ARM_MASK_OP << ARM_SHIFT_OP)))
			|| (PUSH_NUM_LIT7 == (*call_addr &  (ARM_MASK_OP << ARM_SHIFT_OP))))
		{
#ifdef __armv6l__
			if (PUSH_NUM_LIT2 == (*call_addr & ((ARM_MASK_OP << ARM_SHIFT_OP) | (ARM_MASK_REG << ARM_SHIFT_RN))))
			{
				call_addr += 2;			/* Skip over the branch and constant on the next 2 lines */
			}
#endif
			call_addr++;
		}
		/* If it's not a transfer table call, give up.  */
		if ((LOAD_XFER_ADDR != (*call_addr & ~(ARM_MASK_IMM12 << ARM_SHIFT_IMM12)))
			|| (XFER_CALL != *(call_addr + 1)))
		{
			return ((zb_code *)addr);
		}
		xfer_addr = call_addr;
		xfer_offset = (*xfer_addr >> ARM_SHIFT_IMM12) & ARM_MASK_IMM12;
		if ((xf_linefetch * SIZEOF(int4) != xfer_offset) && (xf_zbfetch * SIZEOF(int4) != xfer_offset))
		{
			return ((zb_code *)addr);
		}
	} else if ((LOAD_XFER_ADDR == (*call_addr & ~(ARM_MASK_IMM12 << ARM_SHIFT_IMM12)))
			&& (XFER_CALL == *(call_addr + 1)))
	{
		xfer_addr = call_addr;
		xfer_offset = (*xfer_addr >> ARM_SHIFT_IMM12) & ARM_MASK_IMM12;
		if ((xf_linestart * SIZEOF(int4) != xfer_offset) && (xf_zbstart * SIZEOF(int4) != xfer_offset))
		{
			return ((zb_code *)addr);
		}
	}
	return ((zb_code *)xfer_addr);

}
