/****************************************************************
 *								*
 *	Copyright 2007, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include <emit_code_sp.h>
#include "xfer_enum.h"
#include "i386.h"
#include <rtnhdr.h>	/* Needed by zbreak.h */
#include "zbreak.h"

int inst_size = 0;
/* inst_size is REX + OP_CODE + immed32 used to go past the instruction*/
#define MOV_INTO_ARGREG(call_addr,inst_op)  							\
						(						\
						(inst_size = 1+1+4) &&				\
						(						\
						(*(call_addr) == inst_op + I386_REG_RDI) || 	\
						(*(call_addr) == inst_op + I386_REG_RSI) || 	\
						(*(call_addr) == inst_op + I386_REG_RDX) || 	\
						(*(call_addr) == inst_op + I386_REG_RCX) || 	\
						(*(call_addr) == inst_op + I386_REG_R8) || 	\
						(*(call_addr) == inst_op + I386_REG_R9) || 	\
						(*(call_addr) == inst_op + I386_REG_RAX)  	\
						)						\
						)


/* inst_size is REX + OP_CODE + MODRM + SIB + offset_32/offset_8 used to go past the instruction*/
#define MOV_INTO_STACK(call_addr,inst_op)										\
				(											\
				(*(call_addr) == inst_op) && 								\
				(inst_size = 1 + 1 + 1 + 1 + 1) && (temp_modrm_byte.byte = *(call_addr + 1)) &&		\
				(temp_modrm_byte.modrm.r_m == emit_base_info.modrm_byte.modrm.r_m) &&			\
				(temp_modrm_byte.modrm.reg_opcode == emit_base_info.modrm_byte.modrm.reg_opcode) &&	\
				(											\
				(I386_MOD32_BASE_DISP_8 == temp_modrm_byte.modrm.mod) ||				\
				( (I386_MOD32_BASE_DISP_32 == temp_modrm_byte.modrm.mod) && (inst_size += 3)	)	\
				) &&											\
				(*(call_addr+2) == emit_base_info.sib_byte.byte)					\
				)

zb_code  *find_line_call(void *addr)
{
	unsigned char	temp_char, *call_addr;
	modrm_byte_type	modrm_byte, temp_modrm_byte;

	call_addr = (unsigned char *)addr;
	modrm_byte.byte = *(call_addr + 1);
	if ((I386_INS_Grp5_Prefix == *call_addr) && (I386_INS_CALL_Ev == modrm_byte.modrm.reg_opcode))
	{
		call_addr++;
		assert(I386_REG_EBX == modrm_byte.modrm.r_m);
		call_addr++;
		if (I386_MOD32_BASE_DISP_32 == modrm_byte.modrm.mod)
		{
			if ((xf_linestart * SIZEOF(INTPTR_T) == *((int4 *)call_addr)) ||
			    (xf_zbstart * SIZEOF(INTPTR_T) == *((int4 *)call_addr)))
				return (zb_code *)call_addr;
			return (zb_code *)addr;
		} else if (I386_MOD32_BASE_DISP_8 == modrm_byte.modrm.mod)
		{
			if ((xf_linestart * SIZEOF(INTPTR_T) == *((char *)call_addr)) ||
			    (xf_zbstart * SIZEOF(INTPTR_T) == *((char *)call_addr)))
				return (zb_code *)call_addr;
			call_addr += 1;
		}
	}
	emit_base_offset(I386_REG_SP, 0x12);
	emit_base_info.modrm_byte.modrm.reg_opcode = I386_REG_RAX;
	inst_size = 0;
	if (MOV_INTO_ARGREG(call_addr + 1, I386_INS_MOV_eAX) || MOV_INTO_STACK(call_addr + 1, I386_INS_MOV_Ev_Gv))
	{
		modrm_byte.byte = *(call_addr + 2);
		while (MOV_INTO_ARGREG(call_addr + 1, I386_INS_MOV_eAX) || MOV_INTO_STACK(call_addr + 1, I386_INS_MOV_Ev_Gv))
		{
			assert((8 == inst_size) || (5 == inst_size) || (6 == inst_size)) ;
			call_addr +=  inst_size ;
			inst_size = 0 ;
		}
		modrm_byte.byte = *(call_addr + 1);
		if ((I386_INS_Grp5_Prefix != *call_addr++) || (I386_INS_CALL_Ev != modrm_byte.modrm.reg_opcode))
			return (zb_code *)addr;
		assert((I386_MOD32_BASE_DISP_8 == modrm_byte.modrm.mod) || (I386_MOD32_BASE_DISP_32 == modrm_byte.modrm.mod));
		assert(I386_REG_RBX == modrm_byte.modrm.r_m);
		call_addr++;
		if (I386_MOD32_BASE_DISP_32 == modrm_byte.modrm.mod)
		{
			if ((xf_linefetch * SIZEOF(INTPTR_T) != *(int4 *)call_addr) &&
			    (xf_zbfetch * SIZEOF(INTPTR_T) != *(int4 *)call_addr))
				return (zb_code *)addr;
		} else if (I386_MOD32_BASE_DISP_8 == modrm_byte.modrm.mod)
		{
			if ((xf_linefetch * SIZEOF(INTPTR_T) != *(char *)call_addr) &&
			    (xf_zbfetch * SIZEOF(INTPTR_T) != *(char *)call_addr))
				return (zb_code *)addr;
		}
	} else if ((I386_INS_Grp5_Prefix == *call_addr) && (I386_INS_CALL_Ev == modrm_byte.modrm.reg_opcode))
	{
		modrm_byte.byte = *(call_addr + 1);
		call_addr++;
		assert((I386_MOD32_BASE_DISP_8 == modrm_byte.modrm.mod) || (I386_MOD32_BASE_DISP_32 == modrm_byte.modrm.mod));
		assert(I386_REG_EBX == modrm_byte.modrm.r_m);
		call_addr++;
		if (I386_MOD32_BASE_DISP_32 == modrm_byte.modrm.mod)
		{
			if ((xf_linestart * SIZEOF(INTPTR_T) != *(int4 *)call_addr) &&
			    (xf_zbstart * SIZEOF(INTPTR_T) != *(int4 *)call_addr))
				return (zb_code *)addr;
		} else if (I386_MOD32_BASE_DISP_8 == modrm_byte.modrm.mod)
		{
			if ((xf_linestart * SIZEOF(INTPTR_T) != *(char *)call_addr) &&
			    (xf_zbstart * SIZEOF(INTPTR_T) != *(char *)call_addr))
				return (zb_code *)addr;
		}
	}
	return (zb_code *)call_addr;
}
