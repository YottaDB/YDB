/****************************************************************
 *								*
 *	Copyright 2010, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include <rtnhdr.h>
#include "stack_frame.h"
#include "op.h"
#include "get_ret_targ.h"
#include "xfer_enum.h"
#include "dollar_quit.h"
#if defined(__sparc)
#  include "sparc.h"
#elif defined(__s390__) || defined(__MVS__)
#  include "s390.h"
#elif defined(__hppa)
#  include "hppa.h"
#elif defined(__ia64)
#  include "ia64.h"
#endif

GBLREF	int	process_exiting;

/* Determine value to return for $QUIT:
 *
 *   0 - no return value requested
 *   1 - non-alias return value requested
 *   11 - alias return value requested
 *
 * Determination of parm/no-parm is made by calling get_ret_targ() which checks the stack frames back to
 * a counted frame whether the ret_value field has a return mval, signifying that a return value is required.
 * If a return value is required, determination of the type of return value is made by examining the
 * generated instruction stream at the return point and checking for an OC_EXFUNRET or OC_EXFUNRETALS
 * (non-alias and alias type return var processor respectively) opcode following the return point. This is
 * done by isolating the instruction that indexes into the transfer table, extracting the xfer-table index
 * and checking against known values for op_exfunret and op_exfunretals to determine type of return. No match
 * means no return value.
 *
 * Because this routine looks at the generated code stream at the return point, it is highly platform
 * dependent.
 *
 * Note: If generated code changes for a platform, this module needs to be revisited.
 */
int dollar_quit(void)
{
	stack_frame	*sf;
	int		xfer_index;

	union
	{
		unsigned char	*instr;
		unsigned short	*instr_type;
		unsigned char	*instr_type_8;
		unsigned char	*xfer_offset_8;
		short		*xfer_offset_16;
		int		*xfer_offset_32;
	} ptrs;

	/* There was no return value - return 0 */
	if (NULL == get_ret_targ(&sf))
		return 0;
	/* There is a return value - see if they want a "regular" or alias type return argument */
	sf = sf->old_frame_pointer;		/* Caller's frame */
#	ifdef __i386
	{
		ptrs.instr = sf->mpc;
		/* First figure out the potential length of the lea* instruction loading compiler temp offset */
		if (0x078d == *ptrs.instr_type)
			ptrs.instr += 3;	/* Past the 2 byte lea plus 1 byte push */
		else if (0x478d == *ptrs.instr_type)
			ptrs.instr += 4;	/* Past the 3 byte lea plus 1 byte push */
		else if (0x878d == *ptrs.instr_type)
			ptrs.instr += 7;	/* Past the 6 byte lea plus 1 byte push */
		else
			ptrs.instr = NULL;
		/* Note the "long format call opcode" check below assumes that both of the EXFUNRET[ALS] calls remain at a
		 * greater-than-128 byte offset in the transfer table (which they currently are).
		 */
		if ((NULL != ptrs.instr) && (0x93FF == *ptrs.instr_type))
		{
			ptrs.instr += SIZEOF(*ptrs.instr_type);
			xfer_index = *ptrs.xfer_offset_32 / SIZEOF(void *);
		} else
			xfer_index = -1;
	}
#	elif defined(__x86_64__)
	{
		ptrs.instr = sf->mpc;
		if (0x8d49 == *ptrs.instr_type)
		{
			ptrs.instr += 2;	/* Past first part of instruction type */
			if (0x7e == *ptrs.instr_type_8)
				ptrs.instr += 2;	/* past last byte of instruction type plus 1 byte offset */
			else if (0xbe == *ptrs.instr_type_8)
				ptrs.instr += 5;	/* past last byte of instruction type plus 4 byte offset */
			else
				ptrs.instr = NULL;
		} else
			ptrs.instr_type = NULL;
		if ((NULL != ptrs.instr) && (0x93FF == *ptrs.instr_type))
		{	/* Long format CALL */
			ptrs.instr += SIZEOF(*ptrs.instr_type);
			xfer_index = *ptrs.xfer_offset_32 / SIZEOF(void *);
		} else
			xfer_index = -1;	/* Not an xfer index */
	}
#	elif defined(_AIX)
	{
		ptrs.instr = sf->mpc + 4;	/* Past address load of compiler temp arg */
		if (0xE97C == *ptrs.instr_type)
		{	/* ld of descriptor address from xfer table */
			ptrs.instr += SIZEOF(*ptrs.instr_type);
			xfer_index = *ptrs.xfer_offset_16 / SIZEOF(void *);
		} else
			xfer_index = -1;
	}
#	elif defined(__alpha)	/* Applies to both VMS and Tru64 as have same codegen */
	{
		ptrs.instr = sf->mpc + 4;	/* Past address load of compiler temp arg */
		if (UNIX_ONLY(0xA36C) VMS_ONLY(0xA36B) == *(ptrs.instr_type + 1))	/* Different code for reg diff */
			/* ldl of descriptor address from xfer table - little endian - offset prior to opcode */
			xfer_index = *ptrs.xfer_offset_16 / SIZEOF(void *);
		else
			xfer_index = -1;
	}
#	elif defined(__sparc)
	{
		ptrs.instr = sf->mpc + 4;	/* Past address load of compiler temp arg */
		if (0xC85C == *ptrs.instr_type)
		{	/* ldx of rtn address from xfer table */
			ptrs.instr += SIZEOF(*ptrs.instr_type);
			xfer_index = (*ptrs.xfer_offset_16 & SPARC_MASK_OFFSET) / SIZEOF(void *);
		} else
			xfer_index = -1;
	}
#	elif defined(__s390__) || defined(__MVS__)
	{
		format_RXY		instr_LG;
		ZOS_ONLY(format_RR	instr_RR;)
		union
		{
			int	offset;
			struct
			{	/* Used to reassemble the offset in the LG instruction */
				int	offset_unused:12;
				int	offset_hi:8;
				int	offset_low:12;
			} instr_LG_bits;
		} RXY;
		/* Need to forward space past address load of compiler temp arg. On zOS, the position of the mpc can
		 * differ. If the origin point is an external call, we have to forward space past the BCR following
		 * the call point. If the origin point is an internal call, the call point is a branch with no
		 * following BCR. So zOS needs to determine if it has to jump over a BCR call first.
		 */
		ZOS_ONLY(memcpy(&instr_RR, sf->mpc, SIZEOF(instr_RR)));
		ptrs.instr = sf->mpc;
		ZOS_ONLY(if ((S390_OPCODE_RR_BCR == instr_RR.opcode) && (0 == instr_RR.r1) && (0 == instr_RR.r2))
			 ptrs.instr += 2);	/* Past BCR 0,0 from external call */
		ptrs.instr += 6;		/* Past address load of compiler temp arg */
		memcpy(&instr_LG, ptrs.instr, SIZEOF(instr_LG));
		if ((S390_OPCODE_RXY_LG == instr_LG.opcode) && (S390_SUBCOD_RXY_LG == instr_LG.opcode2)
		    && (GTM_REG_SAVE_RTN_ADDR == instr_LG.r1) && (GTM_REG_XFER_TABLE == instr_LG.b2))
		{	/* LG of rtn address from xfer table */
			RXY.offset = 0;
			RXY.instr_LG_bits.offset_hi = instr_LG.dh2;
			RXY.instr_LG_bits.offset_low = instr_LG.dl2;
			xfer_index = RXY.offset / SIZEOF(void *);
		} else
			xfer_index = -1;
	}
#	elif defined(__hppa)
	{
		hppa_fmt_1	instr_LDX;
		union
		{
			int	offset;
			struct
			{
				signed int	high:19;
				unsigned int	low:13;
			} instr_offset;
		} fmt_1;

		ptrs.instr = sf->mpc + 8;	/* Past address load of compiler temp arg plus rtn call to load of xfer
						 * table call with offset in delay slot */
		memcpy(&instr_LDX, ptrs.instr, SIZEOF(instr_LDX));
		if (((HPPA_INS_LDW >> HPPA_SHIFT_OP) == instr_LDX.pop) && (GTM_REG_XFER_TABLE == instr_LDX.b)
		    && (R22 == instr_LDX.t))
		{	/* ldx of rtn address from xfer table */
			fmt_1.instr_offset.low = instr_LDX.im14a;
			fmt_1.instr_offset.high = instr_LDX.im14b;
			xfer_index = fmt_1.offset / SIZEOF(void *);
		} else
			xfer_index = -1;
	}
#	elif defined(__ia64)
	{
		ia64_bundle	xfer_ref_inst;		/* Buffer to put built instruction into */
		ia64_fmt_A4	adds_inst;		/* The actual adds instruction computing xfer reference */
		union
		{
			int	offset;
			struct
			{
#				ifdef BIGENDIAN
				signed int	sign:19;
				unsigned int	imm6d:6;
				unsigned int	imm7b:7;
#				else
				unsigned int	imm7b:7;
				unsigned int	imm6d:6;
				signed int	sign:19;
#				endif
			} instr_offset;
		} imm14;

		ptrs.instr = sf->mpc + 16;	/* Past address load of compiler temp arg */
#		ifdef BIGENDIAN
		xfer_ref_inst.hexValue.aValue = GTM_BYTESWAP_64(((ia64_bundle *)ptrs.instr)->hexValue.aValue);
		xfer_ref_inst.hexValue.bValue = GTM_BYTESWAP_64(((ia64_bundle *)ptrs.instr)->hexValue.bValue);
#		else
		xfer_ref_inst.hexValue.aValue = ((ia64_bundle *)ptrs.instr)->hexValue.aValue;
		xfer_ref_inst.hexValue.bValue = ((ia64_bundle *)ptrs.instr)->hexValue.bValue;
#		endif
		adds_inst.hexValue = xfer_ref_inst.format.inst3;	/* Extract instruction from bundle */
		if ((8 == adds_inst.format.pop) && (2 == adds_inst.format.x2a)
		    && (GTM_REG_XFER_TABLE == adds_inst.format.r3) && (IA64_REG_SCRATCH1 == adds_inst.format.r1))
		{	/* We have an xfer computation instruction. Find the offset to find which opcode */
			imm14.instr_offset.imm7b = adds_inst.format.imm7b;	/* Low order bits */
			imm14.instr_offset.imm6d = adds_inst.format.imm6d;	/* upper bits minus sign */
			imm14.instr_offset.sign = adds_inst.format.sb;		/* Sign bit propagated */
			xfer_index = imm14.offset / SIZEOF(void *);
		} else
			xfer_index = -1;
	}
#	else
#	  error Unsupported Platform
#	endif
	if (xf_exfunret == xfer_index)
		/* Need a QUIT with a non-alias return value */
		return 1;
	else if (xf_exfunretals == xfer_index)
		/* Need a QUIT with an alias return value */
		return 11;
	else
	{	/* Something weird afoot - had parm block can can't locate EXFUNRET[ALS] opcode. This can happen if
		 * a fatal error occurs during a call before the callee stack frame is actually pushed and we are
		 * called during GTM_FATAL_ERROR.* file creation. Assert that this is the case, else, we just pretend
		 * we didn't find a parm block..
		 */
		assert(process_exiting);
		return 0;
	}
}
