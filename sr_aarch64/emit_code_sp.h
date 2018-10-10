/****************************************************************
 *								*
 * Copyright 2003, 2009 Fidelity Information Services, Inc	*
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
#ifndef EMIT_CODE_SP_INCLUDED
#define EMIT_CODE_SP_INCLUDED

#include "gtm_stdlib.h"		/* for "abs" prototype */

#include "aarch64_registers.h"
#include "aarch64_gtm_registers.h"
#include "aarch64.h"

void	emit_base_offset_load(int base, int offset);
void	emit_base_offset_addr(int base, int offset);
int	encode_immed12(int offset);

#ifdef DEBUG
void    format_machine_inst(void);
void	fmt_ains(void);
void	fmt_brdisp(void);
void	fmt_brdispcond(void);
void	fmt_rd(int size);
void	fmt_rd_rn_shift_immr(int size);
void	fmt_rt_rt2_rn_shift_imm7(int size);
void	fmt_rd_rn_imm12(int size);
void	fmt_rd_shift_imm12(int size);
void	fmt_rd_raw_imm16(int size);
void	fmt_rd_raw_imm16_inv(int size);
void	fmt_rd_rm(int size);
void	fmt_rd_rn(int size);
void	fmt_rd_rn_rm(int size);
void	fmt_rd_rn_rm_sxtw(void);
void	fmt_reg(int reg, int size, int z_flag);
void	fmt_rm(int size);
void	fmt_rn(int size);
void	fmt_rn_rm(int size);
void	fmt_rn_raw_imm12(int size);
void	fmt_rn_shift_imm12(int size);
void	fmt_rt(int size, int z_flag);
void	fmt_rt2(int size);
void	fmt_rt_rn_raw_imm12(int size, int mult);
void	tab_to_column(int col);
#endif

#define INST_SIZE (int)SIZEOF(uint4)
#define BRANCH_OFFSET_FROM_IDX(idx_start, idx_end) (idx_end - (idx_start + 1))
#define MAX_BRANCH_CODEGEN_SIZE 32  /* The length in bytes, of the longest form of branch instruction sequence */

#define MAX_12BIT			0xfff
#define MAX_16BIT			0xffff
#define MAX_32BIT			0xffffffff
#define MAX_48BIT			0xffffffffffff
#define STACK_ARG_OFFSET(indx)		(8 * (indx))
#define MACHINE_FIRST_ARG_REG		AARCH64_REG_X0

#define EMIT_BASE_OFFSET_ADDR(base, offset)		emit_base_offset_addr(base, offset)
#define EMIT_BASE_OFFSET_LOAD(base, offset)		emit_base_offset_load(base, offset)
#define EMIT_BASE_OFFSET_EITHER(base, offset, inst)										\
	((inst == GENERIC_OPCODE_LDA) ? emit_base_offset_addr(base, offset) : emit_base_offset_load(base, offset))

/* Register usage in some of the code generation expansions */
#define GET_ARG_REG(indx)		(AARCH64_REG_X0 + (indx))

/* Define the macros for the instructions to be generated.. */

#define LONG_JUMP_OFFSET		(0x7ffffffc)	/* should be large enough to force the long jump instruction sequence */
#define MAX_OFFSET 			0xffffffff
#define EMIT_JMP_ADJUST_BRANCH_OFFSET

/* Define the macros for the instructions to be generated.. */

#define GENERIC_OPCODE_BEQ		((uint4)AARCH64_INS_BEQ)
#define GENERIC_OPCODE_BGE		((uint4)AARCH64_INS_BGE)
#define GENERIC_OPCODE_BGT		((uint4)AARCH64_INS_BGT)
#define GENERIC_OPCODE_BLE		((uint4)AARCH64_INS_BLE)
#define GENERIC_OPCODE_BLT		((uint4)AARCH64_INS_BLT)
#define GENERIC_OPCODE_BNE		((uint4)AARCH64_INS_BNE)
#define GENERIC_OPCODE_BR		((uint4)AARCH64_INS_B)
#define GENERIC_OPCODE_LOAD		((uint4)AARCH64_INS_LDR_X)
#define GENERIC_OPCODE_STORE		((uint4)AARCH64_INS_STR_X)
#define GENERIC_OPCODE_STORE_ZERO	((uint4)AARCH64_INS_STR_X)
#define GENERIC_OPCODE_LDA		((uint4)AARCH64_INS_ADD_REG)
#define GENERIC_OPCODE_NOP		((uint4)AARCH64_INS_NOP)

/* Branch has origin of +0 instructions. However, if the branch was nullified 
 * in an earlier shrink_trips, the origin is the current instruction itself
*/

/* Define the macros for the instructions to be generated.. */

#define CODE_BUF_GEN_DN_IMM12(ins, areg, breg, imm)	/* add, sub imm */							\
	(ins | (areg << AARCH64_SHIFT_RD) | (breg << AARCH64_SHIFT_RN) | (imm << AARCH64_SHIFT_IMM12))

#define CODE_BUF_GEN_TN_IMM9(ins, areg, breg, imm)										\
	(ins | areg << AARCH64_SHIFT_RT | breg << AARCH64_SHIFT_RN | ((imm) & AARCH64_MASK_IMM9) << AARCH64_SHIFT_IMM9)

#define CODE_BUF_GEN_TN2_IMM7(ins, areg, breg, creg, imm)									\
	(ins | areg << AARCH64_SHIFT_RT | breg << AARCH64_SHIFT_RT2 | creg << AARCH64_SHIFT_RN | ((imm) >> 3) << AARCH64_SHIFT_IMM7)

#define CODE_BUF_GEN_D_IMM16(ins, reg, imm)											\
	(ins | reg << AARCH64_SHIFT_RD | (imm) << AARCH64_SHIFT_IMM16)

#define CODE_BUF_GEN_D_IMM16_SHIFT(ins, reg, imm, shift)									\
	(ins | reg << AARCH64_SHIFT_RD | (imm) << AARCH64_SHIFT_IMM16 | shift << AARCH64_HW_SHIFT)

#define CODE_BUF_GEN_N(ins, reg)												\
	(ins | reg << AARCH64_SHIFT_RN)

#define CODE_BUF_GEN_NM(ins, areg, breg)											\
	(ins | areg << AARCH64_SHIFT_RN | breg << AARCH64_SHIFT_RM | 0x1f)

#define CODE_BUF_GEN_DN_IMMR(ins, areg, breg, imm)										\
	(ins | areg << AARCH64_SHIFT_RD | breg << AARCH64_SHIFT_RN | (imm) << AARCH64_SHIFT_IMMR)

#define CODE_BUF_GEN_DM(ins, areg, breg)											\
	(ins | areg << AARCH64_SHIFT_RD | breg << AARCH64_SHIFT_RM)

#define CODE_BUF_GEN_DNM(ins, areg, breg, creg)											\
	(ins | areg << AARCH64_SHIFT_RD | breg << AARCH64_SHIFT_RN | creg << AARCH64_SHIFT_RM)

#define CODE_BUF_GEN_DNM_IMM6(ins, areg, breg, creg, imm, type)									\
	(ins | areg << AARCH64_SHIFT_RD | breg << AARCH64_SHIFT_RN | creg << AARCH64_SHIFT_RM | (imm) << AARCH64_SHIFT_IMM6 | type << AARCH64_SHIFT_TYPE_SHIFT)

#define CODE_BUF_GEN_TN0_IMM12(ins, areg, breg, imm)										\
	(ins | areg << AARCH64_SHIFT_RN | breg << AARCH64_SHIFT_RN | (imm) << AARCH64_SHIFT_IMM12)

/* LDR_X -- since it's divide by 8 */
#define CODE_BUF_GEN_TN1_IMM12(ins, areg, breg, imm)										\
	(ins | areg << AARCH64_SHIFT_RN | breg << AARCH64_SHIFT_RN | ((imm) >> 3) << AARCH64_SHIFT_IMM12)

/* LDR_W, LDRSW -- since it's divide by 4 */
#define CODE_BUF_GEN_TN2_IMM12(ins, areg, breg, imm)										\
	(ins | areg << AARCH64_SHIFT_RN | breg << AARCH64_SHIFT_RN | ((imm) >> 2) << AARCH64_SHIFT_IMM12)

#define CODE_BUF_GEN_D_IMM12(ins, areg, imm)											\
	(ins | areg << AARCH64_SHIFT_RD | ((imm) >> 3) << AARCH64_SHIFT_IMM12)

/* Macros to create specific generated code sequences */
#define GEN_CLEAR_TRUTH														\
	code_buf[code_idx++] = CODE_BUF_GEN_DN_IMM12(AARCH64_INS_STR_W, AARCH64_REG_ZERO, GTM_REG_DOLLAR_TRUTH, 0);

#define GEN_SET_TRUTH														\
{																\
	code_buf[code_idx++] = CODE_BUF_GEN_D_IMM16(AARCH64_INS_MOV_IMM, GTM_REG_CODEGEN_TEMP, 1);				\
	code_buf[code_idx++] = CODE_BUF_GEN_DN_IMM12(AARCH64_INS_STR_W, GTM_REG_CODEGEN_TEMP, GTM_REG_DOLLAR_TRUTH, 0);		\
}

#define GEN_LOAD_ADDR(areg, breg, disp)												\
{																\
	if (4096 > abs(disp))													\
	{															\
		if (0 < disp)													\
		{														\
			code_buf[code_idx++] = CODE_BUF_GEN_DN_IMM12(AARCH64_INS_ADD_IMM, areg, breg, disp);			\
		} else if (0 > disp)												\
		{														\
			code_buf[code_idx++] = CODE_BUF_GEN_DN_IMM12(AARCH64_INS_SUB_IMM, areg, breg, -1 * disp);		\
		} else if (areg != breg)											\
		{														\
			code_buf[code_idx++] = CODE_BUF_GEN_2REG(AARCH64_INS_MOV_REG, areg, breg);				\
		}														\
	} else															\
	{															\
		code_buf[code_idx++] = CODE_BUF_GEN_D_IMM16(AARCH64_INS_MOV_IMM, GTM_REG_CODEGEN_TEMP, disp);			\
		if ((MAX_16BIT < disp) || (-MAX_16BIT > disp)) 									\
		{														\
			code_buf[code_idx++] = CODE_BUF_GEN_D_IMM16_SHIFT(AARCH64_INS_MOVK, GTM_REG_CODEGEN_TEMP, (disp & 0xfff000) >> 16, 1); \
		}														\
		code_buf[code_idx++] = CODE_BUF_GEN_DNM(AARCH64_INS_ADD_REG, areg, breg, GTM_REG_CODEGEN_TEMP);			\
	}															\
}

/* load 4 bytes */
/* Need to have tmporary value for breg in case the macro is called with "gtm_reg(*inst++)" as breg. Now
 * the increment is only done once for the assignment.
 */
#define GEN_LOAD_WORD_4(areg, breg, disp)											\
{																\
	int tmp_reg;														\
	tmp_reg = breg;														\
	if ((-4096 < disp) && (4096 > disp))											\
	{															\
		code_buf[code_idx++] = CODE_BUF_GEN_DN_IMM12(AARCH64_INS_LDR_W, areg, tmp_reg, disp);				\
	} else															\
	{															\
		code_buf[code_idx++] = CODE_BUF_GEN_D_IMM16(AARCH64_INS_MOV_IMM, GTM_REG_CODEGEN_TEMP_1, disp);			\
		if ((MAX_16BIT < disp) || (-MAX_16BIT > disp)) 									\
		{														\
			code_buf[code_idx++] = CODE_BUF_GEN_D_IMM16_SHIFT(AARCH64_INS_MOVK, GTM_REG_CODEGEN_TEMP_1, (disp & 0xffff0000) >> 16, 0x1); \
		}														\
		code_buf[code_idx++] = CODE_BUF_GEN_DNM(AARCH64_INS_ADD_REG, tmp_reg, tmp_reg, GTM_REG_CODEGEN_TEMP_1);		\
		code_buf[code_idx++] = CODE_BUF_GEN_DN_IMM12(AARCH64_INS_LDR_W, areg, tmp_reg, 0);				\
	}															\
}

/* load 8 bytes (use x registers instead of w) */
#define GEN_LOAD_WORD_8(areg, breg, disp)											\
{																\
	if (4096 > abs(disp))													\
	{															\
		code_buf[code_idx++] = CODE_BUF_GEN_DN_IMM12(AARCH64_INS_LDR_X, areg, breg, disp);				\
	} else															\
	{															\
		code_buf[code_idx++] = CODE_BUF_GEN_D_IMM16(AARCH64_INS_MOV_IMM, GTM_REG_CODEGEN_TEMP, disp);			\
		if ((MAX_16BIT < disp) || (-MAX_16BIT > disp)) 									\
		{														\
			code_buf[code_idx++] = CODE_BUF_GEN_D_IMM16_SHIFT(AARCH64_INS_MOVK, GTM_REG_CODEGEN_TEMP, (disp &0xffff0000) >> 16, 1); \
		}														\
		code_buf[code_idx++] = CODE_BUF_GEN_DNM(AARCH64_INS_ADD_REG, breg, breg, GTM_REG_CODEGEN_TEMP);			\
		code_buf[code_idx++] = CODE_BUF_GEN_DN_IMM12(AARCH64_INS_LDR_X, areg, breg, 0);					\
	}															\
}

/* store 4 bytes */
#define GEN_STORE_WORD_4(areg, breg, disp)											\
{																\
	if ((-4096 < disp) && (4096 > disp))											\
	{															\
		code_buf[code_idx++] = CODE_BUF_GEN_DN_IMM12(AARCH64_INS_STR_W, areg, breg, disp);				\
	} else															\
	{															\
		code_buf[code_idx++] = CODE_BUF_GEN_D_IMM16(AARCH64_INS_MOV_IMM, GTM_REG_CODEGEN_TEMP, disp);			\
		if ((MAX_16BIT < disp) || (-4096 >= disp))									\
		{														\
			code_buf[code_idx++] = CODE_BUF_GEN_D_IMM16_SHIFT(AARCH64_INS_MOVK, GTM_REG_CODEGEN_TEMP, (disp & 0xffff0000) >> 16, 1); \
		}														\
		code_buf[code_idx++] = CODE_BUF_GEN_DNM(AARCH64_INS_ADD_REG, breg, breg, GTM_REG_CODEGEN_TEMP);			\
		code_buf[code_idx++] = CODE_BUF_GEN_DN_IMM12(AARCH64_INS_STR_W, areg, breg, 0);					\
	}															\
}

#define GEN_LOAD_IMMED(reg, imval)												\
{																\
	if (0 <= imval)														\
	{															\
		code_buf[code_idx++] = CODE_BUF_GEN_D_IMM16(AARCH64_INS_MOV_IMM, reg, imval & 0xffff);				\
		if (MAX_16BIT < imval)												\
		{														\
			code_buf[code_idx++] = CODE_BUF_GEN_D_IMM16_SHIFT(AARCH64_INS_MOVK, reg, (imval & 0xffff0000) >> 16, 1);\
		}														\
	} else															\
	{															\
		if (-1 * MAX_16BIT < imval)											\
		{														\
			code_buf[code_idx++] = CODE_BUF_GEN_D_IMM16(AARCH64_INS_MOV_INV, reg, (-1 * imval) - 1); 		\
		} else														\
		{														\
			code_buf[code_idx++] = CODE_BUF_GEN_D_IMM16(AARCH64_INS_MOV_INV, reg, ((-1 * imval) - 1) & 0xffff);	\
			code_buf[code_idx++] = CODE_BUF_GEN_D_IMM16_SHIFT(AARCH64_INS_MOVK, reg, (imval & 0xffff0000) >> 16, 1);\
		}														\
	}															\
}

#define GEN_CLEAR_WORD_EMIT(reg)	emit_trip(*(fst_opr + *inst++), TRUE, GENERIC_OPCODE_STORE_ZERO, reg)
#define GEN_LOAD_WORD_EMIT(reg)		emit_trip(*(fst_opr + *inst++), TRUE, GENERIC_OPCODE_LOAD, reg)

#define GEN_SUBTRACT_REGS(src1, src2, trgt)											\
	code_buf[code_idx++] = CODE_BUF_GEN_DNM(AARCH64_INS_SUB_REG | 0x20000000, trgt, src1, src2);

#define GEN_ADD_IMMED(reg, imval)												\
{																\
	if (0 < imval)														\
	{															\
		if (MAX_12BIT > imval)												\
		{														\
			code_buf[code_idx++] = CODE_BUF_GEN_DN_IMM12(AARCH64_INS_ADD_IMM, reg, reg, imval);			\
		} else														\
		{														\
			code_buf[code_idx++] = CODE_BUF_GEN_D_IMM16(AARCH64_INS_MOV_IMM, GTM_REG_CODEGEN_TEMP, imval);		\
			if (MAX_16BIT < imval)											\
			{													\
			  code_buf[code_idx++] = CODE_BUF_GEN_D_IMM16_SHIFT(AARCH64_INS_MOVK, GTM_REG_CODEGEN_TEMP, (imval & 0xffff0000) >> 16, 1); \
			}													\
			code_buf[code_idx++] = CODE_BUF_GEN_DNM(AARCH64_INS_ADD_REG, reg, reg, GTM_REG_CODEGEN_TEMP);		\
		}														\
	} else if (0 > imval)													\
	{															\
		if (MAX_12BIT > (-1 * imval))											\
		{														\
			code_buf[code_idx++] = CODE_BUF_GEN_DN_IMM12(AARCH64_INS_SUB_IMM, reg, reg, (-1 * imval));		\
		} else														\
		{														\
			code_buf[code_idx++] = CODE_BUF_GEN_D_IMM16(AARCH64_INS_MOV_IMM, GTM_REG_CODEGEN_TEMP, (-1 * imval));	\
			if (MAX_16BIT < (-1 * imval))										\
			{													\
				code_buf[code_idx++] = CODE_BUF_GEN_D_IMM16_SHIFT(AARCH64_INS_MOVK, GTM_REG_CODEGEN_TEMP, ((-1 * imval) * 0xffff0000) >> 16, 1); \
			}													\
			code_buf[code_idx++] = CODE_BUF_GEN_DNM(AARCH64_INS_SUB_REG, reg, reg, GTM_REG_CODEGEN_TEMP);		\
		}														\
	}															\
}

#define GEN_JUMP_REG(reg)													\
	code_buf[code_idx++] = (AARCH64_INS_BR | (reg << AARCH64_SHIFT_RN))

#define GEN_STORE_ARG(reg, offset)												\
{																\
	if (((CGP_APPROX_ADDR == cg_phase || CGP_ADDR_OPT == cg_phase) && (MACHINE_REG_ARGS == vax_pushes_seen))		\
	    || ((CGP_ASSEMBLY == cg_phase || CGP_MACHINE == cg_phase) && (0 == vax_pushes_seen)))				\
	{															\
		code_buf[code_idx++] = CODE_BUF_GEN_DN_IMM12(AARCH64_INS_ADD_IMM, AARCH64_REG_FP, AARCH64_REG_SP, 0);		\
		code_buf[code_idx++] = CODE_BUF_GEN_D_IMM16(AARCH64_INS_MOV_IMM, GTM_REG_CODEGEN_TEMP, offset);			\
		if (MAX_16BIT < offset)												\
		{														\
			code_buf[code_idx++] = CODE_BUF_GEN_D_IMM16_SHIFT(AARCH64_INS_MOVK, GTM_REG_CODEGEN_TEMP,		\
									  (offset & 0xffff0000) >> 16, 1);			\
		}														\
		/* Divide offset by 16, add 1, and multiply it by 16 -- multiply is done within the subtract */			\
		/* The ((offset / 16) + 1) * 16 is to ensure 16 byte stack alignment */						\
		code_buf[code_idx++] = CODE_BUF_GEN_DN_IMMR(AARCH64_INS_LSR, GTM_REG_CODEGEN_TEMP, GTM_REG_CODEGEN_TEMP, 4);	\
		code_buf[code_idx++] = CODE_BUF_GEN_DN_IMM12(AARCH64_INS_ADD_IMM, GTM_REG_CODEGEN_TEMP, GTM_REG_CODEGEN_TEMP, 1);\
		code_buf[code_idx++] = CODE_BUF_GEN_DNM_IMM6(AARCH64_INS_SUB_XREG, AARCH64_REG_SP, AARCH64_REG_SP,		\
							     GTM_REG_CODEGEN_TEMP, 4, AARCH64_SHIFT_TYPE_LSL);			\
	}															\
	code_buf[code_idx++] = CODE_BUF_GEN_TN1_IMM12(AARCH64_INS_STR_X, reg, AARCH64_REG_SP, offset);				\
}

/* Use ldp/stp to copy two 8 byte registers at a time */
#define GEN_MVAL_COPY(src_reg, trg_reg, size)											\
{																\
	for (words_to_move = size / (2 * SIZEOF(UINTPTR_T));  0 < words_to_move; words_to_move--)				\
        {															\
		code_buf[code_idx++] = CODE_BUF_GEN_TN2_IMM7(AARCH64_INS_LDP, GTM_REG_CODEGEN_TEMP, GTM_REG_CODEGEN_TEMP_1,	\
						src_reg, 16);									\
		code_buf[code_idx++] = CODE_BUF_GEN_TN2_IMM7(AARCH64_INS_STP, GTM_REG_CODEGEN_TEMP, GTM_REG_CODEGEN_TEMP_1,	\
						trg_reg, 16);									\
	}															\
	if (0 < words_to_move)													\
	{															\
		code_buf[code_idx++] = CODE_BUF_GEN_DN_IMM12(AARCH64_INS_LDR_X, GTM_REG_CODEGEN_TEMP, src_reg, 0);		\
		code_buf[code_idx++] = CODE_BUF_GEN_DN_IMM12(AARCH64_INS_STR_X, GTM_REG_CODEGEN_TEMP, trg_reg, 0);		\
	}															\
}

#define GEN_PCREL														\
  code_buf[code_idx++] = CODE_BUF_GEN_D_IMM16(AARCH64_INS_ADR, GTM_REG_CODEGEN_TEMP, 0);

#define GEN_MOVE_REG(trg, src)													\
  code_buf[code_idx++] = CODE_BUF_GEN_DM(AARCH64_INS_MOV_REG, trg, src);

/*	CALL_INST_SIZE is the byte length of the minimum-length instruction
 *	sequence to implement a transfer table call.  In the case of AARCH64 it is
 *	the sequence:
 *
 *		(ldr	x23, =xfer_table)
 *	; if offset < 4096
 *		ldr	x15, [x23, #offset]
 *		blr	x15			; call location in xfer_table
 *	; if offset >= 4096
 *		mov	w15, offset & 0xffff
 *		movk	w15, offset >> 16
 *		add	x15, x23, w15, sxtw
 *		ldr	x15, [x15]
 *		blr	x15			; call location in xfer_table
 *
 *	This value is used to determine how to adjust the offset value for a
 *	relative call.
 */

#define CALL_INST_SIZE (2 * INST_SIZE)

#define GEN_XFER_TBL_CALL(xfer)													\
{																\
	GEN_LOAD_WORD_8(GTM_REG_CODEGEN_TEMP, GTM_REG_XFER_TABLE, (xfer >> 3));							\
	code_buf[code_idx++] = CODE_BUF_GEN_N(AARCH64_INS_BLR, GTM_REG_CODEGEN_TEMP);						\
	if (MACHINE_REG_ARGS < vax_pushes_seen)											\
	{															\
	  code_buf[code_idx++] = CODE_BUF_GEN_DN_IMM12(AARCH64_INS_ADD_IMM, AARCH64_REG_SP, AARCH64_REG_FP, 0);			\
	}															\
}

#define GEN_CMP_REG_IMM(reg, imm)												\
{ 																\
	if (0 > imm)														\
	{															\
		if (-4096 < imm)												\
		{														\
			code_buf[code_idx++] = CODE_BUF_GEN_TN1_IMM12(AARCH64_INS_CMN_IMM, reg, reg, -1 * imm);			\
		} else														\
		{														\
			code_buf[code_idx++] = CODE_BUF_GEN_D_IMM16(AARCH64_INS_MOV_IMM, GTM_REG_CODEGEN_TEMP, -1 * imm);	\
			if (MAX_16BIT < (-1 * imm))										\
			{													\
				code_buf[code_idx++] = CODE_BUF_GEN_D_IMM16_SHIFT(AARCH64_INS_MOVK, GTM_REG_CODEGEN_TEMP, ((-1 * imm) & 0xffff0000) >> 16, 1); \
			}													\
			code_buf[code_idx++] = CODE_BUF_GEN_NM(AARCH64_INS_CMN_REG, reg, GTM_REG_CODEGEN_TEMP);			\
		}														\
	} else															\
	{															\
		if (4096 > imm)													\
		{														\
			code_buf[code_idx++] = CODE_BUF_GEN_TN1_IMM12(AARCH64_INS_CMP_IMM, reg, reg, imm);			\
		} else														\
		{														\
			code_buf[code_idx++] = CODE_BUF_GEN_D_IMM16(AARCH64_INS_MOV_IMM, GTM_REG_CODEGEN_TEMP, imm);		\
			if (MAX_16BIT < imm)											\
			{													\
				code_buf[code_idx++] = CODE_BUF_GEN_D_IMM16_SHIFT(AARCH64_INS_MOVK, GTM_REG_CODEGEN_TEMP, (imm & 0xffff0000) >> 16, 1); \
			}													\
			code_buf[code_idx++] = CODE_BUF_GEN_NM(AARCH64_INS_CMP_REG, reg,GTM_REG_CODEGEN_TEMP);			\
		}														\
	}															\
}

/* Macros to return an instruction value. This is typically used to modify an instruction
   that is already in the instruction buffer such as the last instruction that was created
   by emit_pcrel().
*/
#define IGEN_COND_BRANCH_REG_OFFSET(opcode, reg, disp)	(opcode | (((disp & AARCH64_MASK_BRANCH_DISP) >> 2) << AARCH64_SHIFT_BRANCH_DISP))
#define IGEN_UCOND_BRANCH_REG_OFFSET(opcode, reg, disp) IGEN_COND_BRANCH_REG_OFFSET(opcode, reg, disp)
#define IGEN_LOAD_ADDR_REG(reg)				(AARCH64_INS_ADD_REG_EXT | ((reg & AARCH64_MASK_REG) << AARCH64_SHIFT_RD))
#define IGEN_LOAD_WORD_REG_4(reg)			(AARCH64_INS_LDRSW | (reg & AARCH64_MASK_REG) << AARCH64_SHIFT_RT)
#define IGEN_LOAD_WORD_REG_8(reg)			(AARCH64_INS_LDR_X | (reg & AARCH64_MASK_REG) << AARCH64_SHIFT_RT)
#define IGEN_STORE_WORD_REG_4(reg)			(AARCH64_INS_STR_W | (reg & AARCH64_MASK_REG) << AARCH64_SHIFT_RT)
#define IGEN_STORE_WORD_REG_8(reg)			(AARCH64_INS_STR_X | (reg & AARCH64_MASK_REG) << AARCH64_SHIFT_RT)
#define IGEN_COND_BRANCH_OFFSET(disp)			((disp & AARCH64_MASK_BRANCH_DISP) << AARCH64_SHIFT_BRANCH_DISP)
#define IGEN_LOAD_LINKAGE(reg)				(AARCH64_INS_SUB | ((reg & AARCH64_MASK_REG) << AARCH64_SHIFT_RT))
#define IGEN_LOAD_NATIVE_REG(reg)												\
{																\
	code_buf[code_idx] = ADJ_OFFSET_FOR_8(code_buf[code_idx]);								\
	code_buf[code_idx++] |= (AARCH64_INS_LDR_X | (reg & AARCH64_MASK_REG) << AARCH64_SHIFT_RT);				\
}

#define CLEAR_INST_OFFSET(instr)			(instr & (~(AARCH64_MASK_IMM12 << AARCH64_SHIFT_IMM12)))
#define ADJ_INST_OFFSET(instr, shift)			(((instr & (AARCH64_MASK_IMM12 << AARCH64_SHIFT_IMM12)) >> shift)	\
								& (AARCH64_MASK_IMM12 << AARCH64_SHIFT_IMM12))

#define ADJ_OFFSET_FOR_4(instr)				(CLEAR_INST_OFFSET(instr) | ADJ_INST_OFFSET(instr, 2))
#define ADJ_OFFSET_FOR_8(instr)				(CLEAR_INST_OFFSET(instr) | ADJ_INST_OFFSET(instr, 3))

#define IGEN_GENERIC_REG(opcode, reg)												\
{																\
	switch(opcode)														\
	{															\
		case GENERIC_OPCODE_LDA:											\
			code_buf[code_idx++] |= IGEN_LOAD_ADDR_REG(reg);							\
			break;													\
		case GENERIC_OPCODE_LOAD:											\
			if (4 == next_ptr_offset)										\
			  {	/* Divide offset by 4 */									\
				code_buf[code_idx] = ADJ_OFFSET_FOR_4(code_buf[code_idx]);					\
				code_buf[code_idx++] |= IGEN_LOAD_WORD_REG_4(reg);						\
			} else													\
			  {	/* Divide offset by 8 */									\
				code_buf[code_idx] = ADJ_OFFSET_FOR_8(code_buf[code_idx]);					\
				code_buf[code_idx++] |= IGEN_LOAD_WORD_REG_8(reg);						\
			}													\
			break;													\
		/* case GENERIC_OPCODE_STORE_ZERO:*/										\
		case GENERIC_OPCODE_STORE:											\
			if (4 == next_ptr_offset)										\
			  {	/* Divide offset by 4 */									\
				code_buf[code_idx] = ADJ_OFFSET_FOR_4(code_buf[code_idx]);					\
				code_buf[code_idx++] |= IGEN_STORE_WORD_REG_4(reg);						\
			} else													\
			  {	/* Divide offset by 8 */									\
				code_buf[code_idx] = ADJ_OFFSET_FOR_8(code_buf[code_idx]);					\
				code_buf[code_idx++] |= IGEN_STORE_WORD_REG_8(reg);						\
			}													\
			break;													\
		default: /* which opcode ? */											\
			assertpro(FALSE);											\
			break;													\
	}															\
}

/* Some macros that are used in certain routines in emit_code.c. The names of
 * these macros start with the routine name they are used in.
*/

/* Can jump be done within range of immediate operand */
/* Multiples of 4 in the range +/-128MB (0xffffffc) */
#define EMIT_JMP_SHORT_CODE_CHECK												\
	((branch_offset >= -268435452) && (branch_offset <= 268435452))

/* Emit the short jump */
#define EMIT_JMP_SHORT_CODE_GEN													\
{																\
	if (GENERIC_OPCODE_BR == branchop)											\
	{															\
		code_buf[code_idx++] = (branchop | ((branch_offset & AARCH64_MASK_BRANCH_DISP) << AARCH64_SHIFT_BRANCH_DISP));	\
	} else															\
	{															\
		code_buf[code_idx++] = (branchop | ((branch_offset & AARCH64_MASK_BRANCH_DISP_COND) << AARCH64_SHIFT_BRANCH_DISP_COND)); \
	}															\
}

/* Is this a conditional branch? */
#define EMIT_JMP_OPPOSITE_BR_CHECK												\
		((branchop != AARCH64_INS_B) && (branchop != AARCH64_INS_BEQ))
#define EMIT_JMP_GEN_COMPARE

#define EMIT_JMP_LONG_CODE_CHECK	FALSE

#define EMIT_TRIP_ILIT_GEN		GEN_LOAD_IMMED(trg_reg, immediate)

/*
 * GT.M on AIX and SPARC is 64bit
 * By default the loads/stores use ldd/std(load double),
 * but if the value being dealt with is a word, the
 * opcode in generic_inst is changed to ldw/stw
 * On other platforms, it is defined to null
 */
#define REVERT_GENERICINST_TO_WORD(inst)

#endif
