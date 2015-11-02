/****************************************************************
 *								*
 *	Copyright 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "emit_code_sp.h"
#include "xfer_enum.h"
#include "i386.h"
#include "rtnhdr.h"	/* Needed by zbreak.h */
#include "zbreak.h"

int inst_size = 0 ;
/* inst_size is REX + OP_CODE + immed64 used to go past the instruction*/
#define MOV_INTO_ARGREG(call_addr,inst_op)  							\
						(						\
						(inst_size = 1+1+8) &&				\
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


/* inst_size is REX + OP_CODE + MODRM + SIB + offset_32 used to go past the instruction*/
#define MOV_INTO_STACK(call_addr,inst_op)								\
						(							\
						(inst_size = 1+1+1+1+4) &&				\
						(*(call_addr) == inst_op) && 				\
						(*(call_addr+1) == emit_base_info.modrm_byte.byte) &&	\
						(*(call_addr+2) == emit_base_info.sib_byte.byte)		\
						)
zb_code  *find_line_call(void *addr)
{
	unsigned char *call_addr;
	union
	{
		ModR_M		modrm;
		unsigned char	byte;
	} modrm_byte;

	call_addr = (unsigned char *)addr;
	modrm_byte.byte = *(call_addr + 1);
	if (*call_addr == I386_INS_Grp5_Prefix  &&  modrm_byte.modrm.reg_opcode == I386_INS_CALL_Ev)
	{
		call_addr++;
		assert (modrm_byte.modrm.r_m == I386_REG_EBX);
		call_addr++;
		if (modrm_byte.modrm.mod == I386_MOD32_BASE_DISP_32)
		{
                        if (*call_addr == xf_linestart * sizeof(INTPTR_T) ||
                            *call_addr == xf_zbstart * sizeof(INTPTR_T))
                                return (zb_code *)call_addr;

                        if (*((int4 *)call_addr) != xf_isformal * sizeof(INTPTR_T))
                                return (zb_code *)addr;

			call_addr += sizeof(int4);
		}
	}

	emit_base_offset(I386_REG_SP,0x12345678);
	emit_base_info.modrm_byte.modrm.reg_opcode = I386_REG_RAX ;

	inst_size = 0 ;
	if (MOV_INTO_ARGREG(call_addr+1,I386_INS_MOV_eAX) || MOV_INTO_STACK(call_addr+1,I386_INS_MOV_Ev_Gv) )
	{
		modrm_byte.byte = *(call_addr + 2);

		while (MOV_INTO_ARGREG(call_addr+1,I386_INS_MOV_eAX) || MOV_INTO_STACK(call_addr+1,I386_INS_MOV_Ev_Gv))
                {
			assert((inst_size == 8) || (inst_size == 10)) ;
			call_addr +=  inst_size ;
			inst_size = 0 ;
                }

		modrm_byte.byte = *(call_addr + 1);
		if (*call_addr++ != I386_INS_Grp5_Prefix  ||  modrm_byte.modrm.reg_opcode != I386_INS_CALL_Ev)
			return (zb_code *)addr;

		assert (modrm_byte.modrm.mod == I386_MOD32_BASE_DISP_8  ||  modrm_byte.modrm.mod == I386_MOD32_BASE_DISP_32);
		assert (modrm_byte.modrm.r_m == I386_REG_RBX);
		call_addr++;
		if (modrm_byte.modrm.mod == I386_MOD32_BASE_DISP_32)
		{
			if (*call_addr != xf_linefetch * sizeof(INTPTR_T) &&
			    *call_addr != xf_zbfetch * sizeof(INTPTR_T))
				return (zb_code *)addr;
		}
	}
	else if (*call_addr == I386_INS_Grp5_Prefix  &&  modrm_byte.modrm.reg_opcode == I386_INS_CALL_Ev)
	{
		modrm_byte.byte = *(call_addr + 1);
		call_addr++;
		assert (modrm_byte.modrm.mod == I386_MOD32_BASE_DISP_8  ||  modrm_byte.modrm.mod == I386_MOD32_BASE_DISP_32);
		assert (modrm_byte.modrm.r_m == I386_REG_EBX);
		call_addr++;
		if (modrm_byte.modrm.mod == I386_MOD32_BASE_DISP_32)
		{
			if (*call_addr != xf_linestart * sizeof(INTPTR_T) &&
			    *call_addr != xf_zbstart * sizeof(INTPTR_T))
				return (zb_code *)addr;
		}
	}
	return (zb_code *)call_addr;
}
