/****************************************************************
 *								*
 * Copyright 2003, 2009 Fidelity Information Services, Inc	*
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

#ifndef EMIT_CODE_SP_INCLUDED
#define EMIT_CODE_SP_INCLUDED

#include "gtm_stdlib.h"		/* for "abs" prototype */

#include "arm_registers.h"
#include "arm_gtm_registers.h"
#include "arm.h"

void	emit_base_offset_load(int base, int offset);
void	emit_base_offset_addr(int base, int offset);
int	encode_immed12(int offset);

#ifdef DEBUG
void    format_machine_inst(void);
int	decode_immed12(int offset);
void	fmt_ains(void);
void	fmt_brdisp(void);
void	fmt_const(void);
void	fmt_registers(void);
void	fmt_rd(void);
void	fmt_rd_rm_shift_imm5(void);
void	fmt_rd_shift_imm12(void);
void	fmt_rd_raw_imm16(void);
void	fmt_rd_rm(void);
void	fmt_rd_rn(void);
void	fmt_rd_rn_shift_imm12(void);
void	fmt_rd_rn_rm(void);
void	fmt_reg(int reg);
void	fmt_rm(void);
void	fmt_rn(void);
void	fmt_rn_rm(void);
void	fmt_rn_raw_imm12(void);
void	fmt_rn_shift_imm12(void);
void	fmt_rt(void);
void	fmt_rt_rn_raw_imm12(void);
void	fmt_sregs(void);
void	tab_to_column(int col);
#endif

#define INST_SIZE (int)SIZEOF(uint4)
#define BRANCH_OFFSET_FROM_IDX(idx_start, idx_end) (idx_end - (idx_start + 1))
#define LONG_JUMP_OFFSET (0x7ffffffc) /* should be large enough to force the long jump instruction sequence */
#define MAX_BRANCH_CODEGEN_SIZE 32  /* The length in bytes, of the longest form of branch instruction sequence */

#define MAX_OFFSET 			0xffff
#define MAX_16BIT			0xffff
#define STACK_ARG_OFFSET(indx)		(4 * (indx))
#define MACHINE_FIRST_ARG_REG		ARM_REG_R0

#define EMIT_BASE_OFFSET_ADDR(base, offset)		emit_base_offset_addr(base, offset)
#define EMIT_BASE_OFFSET_LOAD(base, offset)		emit_base_offset_load(base, offset)
#define EMIT_BASE_OFFSET_EITHER(base, offset, inst)										\
	((inst == GENERIC_OPCODE_LDA) ? emit_base_offset_addr(base, offset) : emit_base_offset_load(base, offset))

/* Register usage in some of the code generation expansions */
#define CALLS_TINT_TEMP_REG		ARM_REG_R1
#define CLRL_REG			ARM_REG_R0
#define CMPL_TEMP_REG			ARM_REG_R2
#define GET_ARG_REG(indx)		(ARM_REG_R0 + (indx))
#ifdef __armv7l__
#define MOVC3_SRC_REG			ARM_REG_R2
#define MOVC3_TRG_REG			ARM_REG_R3
#else	/* __armv6l__ */
#define MOVC3_SRC_REG			ARM_REG_R12
#define MOVC3_TRG_REG			ARM_REG_R4
#endif
#define MOVL_RETVAL_REG			ARM_REG_R0
#define MOVL_REG_R1			ARM_REG_R1

/* Define the macros for the instructions to be generated.. */

#define GENERIC_OPCODE_BEQ		((uint4)ARM_INS_BEQ)
#define GENERIC_OPCODE_BGE		((uint4)ARM_INS_BGE)
#define GENERIC_OPCODE_BGT		((uint4)ARM_INS_BGT)
#define GENERIC_OPCODE_BLE		((uint4)ARM_INS_BLE)
#define GENERIC_OPCODE_BLT		((uint4)ARM_INS_BLT)
#define GENERIC_OPCODE_BNE		((uint4)ARM_INS_BNE)
#define GENERIC_OPCODE_BR		((uint4)ARM_INS_B)
#define GENERIC_OPCODE_LOAD		((uint4)ARM_INS_LDR)
#define GENERIC_OPCODE_STORE		((uint4)ARM_INS_STR)
#define GENERIC_OPCODE_LDA		((uint4)ARM_INS_SUB)
#define GENERIC_OPCODE_NOP		((uint4)ARM_INS_NOP)

/* Define the macros for the instructions to be generated.. */


/* Macros to create specific generated code sequences */
#ifdef __armv7l__
#define GEN_LOAD_WORD(areg, breg, disp)												\
{																\
	if ((-4096 < disp) && (4096 > disp))											\
	{															\
		if (0 <= disp)													\
		{														\
			code_buf[code_idx++] = (ARM_INS_LDR									\
							| ARM_U_BIT_ON								\
							| ((disp & ARM_MASK_IMM12) << ARM_SHIFT_IMM12)				\
							| (breg << ARM_SHIFT_RN)						\
							| (areg << ARM_SHIFT_RT));						\
		} else														\
		{														\
			code_buf[code_idx++] = (ARM_INS_LDR									\
							& ARM_U_BIT_OFF								\
							| (((-1 * disp) & ARM_MASK_IMM12) << ARM_SHIFT_IMM12)			\
							| (breg << ARM_SHIFT_RN)						\
							| (areg << ARM_SHIFT_RT));						\
		}														\
	} else															\
	{															\
		code_buf[code_idx++] = (ARM_INS_MOVW										\
						| (GTM_REG_CODEGEN_TEMP << ARM_SHIFT_RD)					\
						| ((disp & ARM_MASK_IMM4_HI) << ARM_SHIFT_IMM4_HI)		 		\
						| ((disp & ARM_MASK_IMM12) << ARM_SHIFT_IMM12));				\
		if ((MAX_16BIT < disp) || (-MAX_16BIT > disp)) 									\
		{														\
			code_buf[code_idx++] = (ARM_INS_MOVT									\
							| (GTM_REG_CODEGEN_TEMP << ARM_SHIFT_RD) 				\
							| (((disp >> 16) & ARM_MASK_IMM4_HI) << ARM_SHIFT_IMM4_HI)		\
							| (((disp >> 16) & ARM_MASK_IMM12) << ARM_SHIFT_IMM12));		\
		}														\
		code_buf[code_idx++] = (ARM_INS_ADD_REG										\
						| (breg << ARM_SHIFT_RD)							\
						| (breg << ARM_SHIFT_RN)							\
						| (GTM_REG_CODEGEN_TEMP << ARM_SHIFT_RM));					\
		code_buf[code_idx++] = (ARM_INS_LDR										\
						| (breg << ARM_SHIFT_RN)							\
						| (areg << ARM_SHIFT_RT));							\
	}															\
}
#else	/* __armv6l__ */
#define GEN_LOAD_WORD(areg, breg, disp)												\
{																\
	if ((-4096 < disp) && (4096 > disp))											\
	{															\
		if (0 <= disp)													\
		{														\
			code_buf[code_idx++] = (ARM_INS_LDR									\
							| ARM_U_BIT_ON								\
							| ((disp & ARM_MASK_IMM12) << ARM_SHIFT_IMM12)				\
							| (breg << ARM_SHIFT_RN)						\
							| (areg << ARM_SHIFT_RT));						\
		} else														\
		{														\
			code_buf[code_idx++] = (ARM_INS_LDR									\
							& ARM_U_BIT_OFF								\
							| (((-1 * disp) & ARM_MASK_IMM12) << ARM_SHIFT_IMM12)			\
							| (breg << ARM_SHIFT_RN)						\
							| (areg << ARM_SHIFT_RT));						\
		}														\
	} else															\
	{															\
		code_buf[code_idx++] = (ARM_INS_LDR										\
						| (ARM_REG_PC << ARM_SHIFT_RN)							\
						| (GTM_REG_CODEGEN_TEMP << ARM_SHIFT_RT));					\
		code_buf[code_idx++] = (ARM_INS_B);										\
		code_buf[code_idx++] = disp;											\
		code_buf[code_idx++] = (ARM_INS_ADD_REG										\
						| (breg << ARM_SHIFT_RD)							\
						| (breg << ARM_SHIFT_RN)							\
						| (GTM_REG_CODEGEN_TEMP << ARM_SHIFT_RM));					\
		code_buf[code_idx++] = (ARM_INS_LDR										\
						| (breg << ARM_SHIFT_RN)							\
						| (areg << ARM_SHIFT_RT));							\
	}															\
}
#endif

#ifdef __armv7l__
#define GEN_STORE_WORD(areg, breg, disp)											\
{																\
	if ((-4096 < disp) && (4096 > disp))											\
	{															\
		if (0 <= disp)													\
		{														\
			code_buf[code_idx++] = (ARM_INS_STR									\
							| ARM_U_BIT_ON								\
							| ((disp & ARM_MASK_IMM12) << ARM_SHIFT_IMM12)				\
							| (breg << ARM_SHIFT_RN)						\
							| (areg << ARM_SHIFT_RT));						\
		} else														\
		{														\
			code_buf[code_idx++] = (ARM_INS_STR									\
							& ARM_U_BIT_OFF								\
							| (((-1 * disp) & ARM_MASK_IMM12) << ARM_SHIFT_IMM12)			\
							| (breg << ARM_SHIFT_RN)						\
							| (areg << ARM_SHIFT_RT));						\
		}														\
	} else															\
	{															\
		code_buf[code_idx++] = (ARM_INS_MOVW										\
						| (GTM_REG_CODEGEN_TEMP << ARM_SHIFT_RD)					\
						| ((disp & ARM_MASK_IMM4_HI) << ARM_SHIFT_IMM4_HI)				\
						| ((disp & ARM_MASK_IMM12) << ARM_SHIFT_IMM12));				\
		if ((MAX_16BIT < disp) || (-4096 >= disp))									\
		{														\
			code_buf[code_idx++] = (ARM_INS_MOVT									\
							| (GTM_REG_CODEGEN_TEMP << ARM_SHIFT_RD) 				\
							| (((disp >> 16) & ARM_MASK_IMM4_HI) << ARM_SHIFT_IMM4_HI)		\
							| (((disp >> 16) & ARM_MASK_IMM12) << ARM_SHIFT_IMM12));		\
		}														\
		code_buf[code_idx++] = (ARM_INS_ADD_REG										\
						| (breg << ARM_SHIFT_RD)							\
						| (breg << ARM_SHIFT_RN)							\
						| (GTM_REG_CODEGEN_TEMP << ARM_SHIFT_RM));					\
		code_buf[code_idx++] = (ARM_INS_STR										\
						| (areg << ARM_SHIFT_RT)							\
						| (breg << ARM_SHIFT_RN));							\
	}															\
}
#else	/* __armv6l__ */
#define GEN_STORE_WORD(areg, breg, disp)											\
{																\
	if ((-4096 < disp) && (4096 > disp))											\
	{															\
		if (0 <= disp)													\
		{														\
			code_buf[code_idx++] = (ARM_INS_STR									\
							| ARM_U_BIT_ON								\
							| ((disp & ARM_MASK_IMM12) << ARM_SHIFT_IMM12)				\
							| (breg << ARM_SHIFT_RN)						\
							| (areg << ARM_SHIFT_RT));						\
		} else														\
		{														\
			code_buf[code_idx++] = (ARM_INS_STR									\
							& ARM_U_BIT_OFF								\
							| (((-1 * disp) & ARM_MASK_IMM12) << ARM_SHIFT_IMM12)			\
							| (breg << ARM_SHIFT_RN)						\
							| (areg << ARM_SHIFT_RT));						\
		}														\
	} else															\
	{															\
		code_buf[code_idx++] = (ARM_INS_LDR										\
						| (ARM_REG_PC << ARM_SHIFT_RN)							\
						| (GTM_REG_CODEGEN_TEMP << ARM_SHIFT_RT));					\
		code_buf[code_idx++] = (ARM_INS_B);										\
		code_buf[code_idx++] = disp;											\
		code_buf[code_idx++] = (ARM_INS_ADD_REG										\
						| (breg << ARM_SHIFT_RD)							\
						| (breg << ARM_SHIFT_RN)							\
						| (GTM_REG_CODEGEN_TEMP << ARM_SHIFT_RM));					\
		code_buf[code_idx++] = (ARM_INS_STR										\
						| (areg << ARM_SHIFT_RT)							\
						| (breg << ARM_SHIFT_RN));							\
	}															\
}
#endif

#ifdef __armv7l__
#define GEN_LOAD_IMMED(reg, imval)												\
{																\
	if (0 <= imval)														\
	{															\
		if (0 <= encode_immed12(imval))											\
		{														\
			code_buf[code_idx++] = (ARM_INS_MOV_IMM									\
							| (encode_immed12(imval) << ARM_SHIFT_IMM12)				\
							| (reg << ARM_SHIFT_RD));						\
		} else														\
		{														\
			code_buf[code_idx++] = (ARM_INS_MOVW									\
							| (reg << ARM_SHIFT_RD)							\
							| ((imval & ARM_MASK_IMM4_HI) << ARM_SHIFT_IMM4_HI)			\
							| ((imval & ARM_MASK_IMM12) << ARM_SHIFT_IMM12));			\
			if (MAX_16BIT < imval)											\
			{													\
				code_buf[code_idx++] = (ARM_INS_MOVT								\
								| (reg << ARM_SHIFT_RD) 					\
								| (((imval >> 16) & ARM_MASK_IMM4_HI) << ARM_SHIFT_IMM4_HI)	\
								| (((imval >> 16) & ARM_MASK_IMM12) << ARM_SHIFT_IMM12));	\
			}													\
		}														\
	} else															\
	{															\
		if (256 >= (-1 * (imval)))											\
		{														\
			code_buf[code_idx++] = (ARM_INS_MVN									\
							| (reg << ARM_SHIFT_RD) 						\
							| ((((-1 * (imval)) - 1) & ARM_MASK_IMM12) << ARM_SHIFT_IMM12));	\
		} else														\
		{														\
			code_buf[code_idx++] = (ARM_INS_MOVW									\
							| (reg << ARM_SHIFT_RD) 						\
							| ((imval & ARM_MASK_IMM4_HI) << ARM_SHIFT_IMM4_HI)			\
							| ((imval & ARM_MASK_IMM12) << ARM_SHIFT_IMM12));			\
			code_buf[code_idx++] = (ARM_INS_MOVT									\
							| (reg << ARM_SHIFT_RD) 						\
							| (((imval >> 16) & ARM_MASK_IMM4_HI) << ARM_SHIFT_IMM4_HI)		\
							| (((imval >> 16) & ARM_MASK_IMM12) << ARM_SHIFT_IMM12));		\
		}														\
	}															\
}
#else	/* __armv6l__ */
#define GEN_LOAD_IMMED(reg, imval)												\
{																\
	if (0 <= imval)														\
	{															\
		if (0 <= encode_immed12(imval))											\
		{														\
			code_buf[code_idx++] = (ARM_INS_MOV_IMM									\
							| (encode_immed12(imval) << ARM_SHIFT_IMM12)				\
							| (reg << ARM_SHIFT_RD));						\
		} else														\
		{														\
			code_buf[code_idx++] = (ARM_INS_LDR									\
							| (ARM_REG_PC << ARM_SHIFT_RN)						\
							| (reg << ARM_SHIFT_RT));						\
			code_buf[code_idx++] = (ARM_INS_B);									\
			code_buf[code_idx++] = imval;										\
		}														\
	} else															\
	{															\
		if (256 >= (-1 * (imval)))											\
		{														\
			code_buf[code_idx++] = (ARM_INS_MVN									\
							| (reg << ARM_SHIFT_RD) 						\
							| ((((-1 * (imval)) - 1) & ARM_MASK_IMM12) << ARM_SHIFT_IMM12));	\
		} else														\
		{														\
			code_buf[code_idx++] = (ARM_INS_LDR									\
							| (ARM_REG_PC << ARM_SHIFT_RN)						\
							| (reg << ARM_SHIFT_RT));						\
			code_buf[code_idx++] = (ARM_INS_B);									\
			code_buf[code_idx++] = imval;										\
		}														\
	}															\
}
#endif

#define GEN_CLEAR_WORD_EMIT(reg)												\
{       															\
		code_buf[code_idx++] = (ARM_INS_MOV_IMM										\
						| (reg << ARM_SHIFT_RD));							\
		emit_trip(*(fst_opr + *inst++), TRUE, ARM_INS_STR, reg);							\
}

#define GEN_LOAD_WORD_EMIT(reg)		emit_trip(*(fst_opr + *inst++), TRUE, ARM_INS_LDR, reg)

#define GEN_SUBTRACT_REGS(src1, src2, trgt)											\
	code_buf[code_idx++] = (ARM_INS_SUB_REG											\
					| ARM_S_BIT_ON										\
					| (src1 << ARM_SHIFT_RN)								\
					| (src2 << ARM_SHIFT_RM)								\
					| (trgt << ARM_SHIFT_RD))

#ifdef __armv7l__
#define GEN_ADD_IMMED(reg, imval)												\
{																\
	if (0 <= encode_immed12(abs(imval)))											\
	{															\
		if (0 < imval)													\
		{														\
			code_buf[code_idx++] = (ARM_INS_ADD_IMM									\
							| reg << ARM_SHIFT_RN							\
							| reg << ARM_SHIFT_RD							\
							| encode_immed12(imval) << ARM_SHIFT_IMM12);				\
		} else if (0 > imval)												\
		{														\
			code_buf[code_idx++] = (ARM_INS_SUB_IMM									\
							| reg << ARM_SHIFT_RN							\
							| reg << ARM_SHIFT_RD							\
							| encode_immed12(-1 * imval) << ARM_SHIFT_IMM12);			\
		}														\
	} else															\
	{															\
		code_buf[code_idx++] = (ARM_INS_MOVW										\
						| (GTM_REG_CODEGEN_TEMP << ARM_SHIFT_RD)					\
						| ((imval & ARM_MASK_IMM4_HI) << ARM_SHIFT_IMM4_HI)				\
						| ((imval & ARM_MASK_IMM12) << ARM_SHIFT_IMM12));				\
		if (MAX_16BIT < imval)												\
		{														\
			code_buf[code_idx++] = (ARM_INS_MOVT									\
							| (GTM_REG_CODEGEN_TEMP << ARM_SHIFT_RD)				\
							| (((imval >> 16) & ARM_MASK_IMM4_HI) << ARM_SHIFT_IMM4_HI)		\
							| (((imval >> 16) & ARM_MASK_IMM12) << ARM_SHIFT_IMM12));		\
		}														\
		code_buf[code_idx++] = (ARM_INS_ADD_REG										\
						| (reg << ARM_SHIFT_RD)								\
						| (reg << ARM_SHIFT_RN)								\
						| (GTM_REG_CODEGEN_TEMP << ARM_SHIFT_RM));					\
	}															\
}
#else	/* __armv6l__ */
#define GEN_ADD_IMMED(reg, imval)												\
{																\
	if (0 <= encode_immed12(abs(imval)))											\
	{															\
		if (0 < imval)													\
		{														\
			code_buf[code_idx++] = (ARM_INS_ADD_IMM									\
							| reg << ARM_SHIFT_RN							\
							| reg << ARM_SHIFT_RD							\
							| encode_immed12(imval) << ARM_SHIFT_IMM12);				\
		} else if (0 > imval)												\
		{														\
			code_buf[code_idx++] = (ARM_INS_SUB_IMM									\
							| reg << ARM_SHIFT_RN							\
							| reg << ARM_SHIFT_RD							\
							| encode_immed12(-1 * imval) << ARM_SHIFT_IMM12);			\
		}														\
	} else															\
	{															\
		code_buf[code_idx++] = (ARM_INS_LDR										\
						| (ARM_REG_PC << ARM_SHIFT_RN)							\
						| (GTM_REG_CODEGEN_TEMP << ARM_SHIFT_RT));					\
		code_buf[code_idx++] = (ARM_INS_B);										\
		code_buf[code_idx++] = imval;											\
		code_buf[code_idx++] = (ARM_INS_ADD_REG										\
						| (reg << ARM_SHIFT_RD)								\
						| (reg << ARM_SHIFT_RN)								\
						| (GTM_REG_CODEGEN_TEMP << ARM_SHIFT_RM));					\
	}															\
}
#endif

#define GEN_JUMP_REG(reg)													\
	code_buf[code_idx++] = (ARM_INS_BX | (reg << ARM_SHIFT_RM))

#ifdef __armv7l__
#define GEN_STORE_ARG(reg, offset)												\
{																\
	if (((CGP_APPROX_ADDR == cg_phase || CGP_ADDR_OPT == cg_phase) && (MACHINE_REG_ARGS == vax_pushes_seen))		\
	    || ((CGP_ASSEMBLY == cg_phase || CGP_MACHINE == cg_phase) && (0 == vax_pushes_seen)))				\
	{															\
		code_buf[code_idx++] = (ARM_INS_MOV_REG										\
						| ARM_REG_SP << ARM_SHIFT_RM							\
						| ARM_REG_FP << ARM_SHIFT_RD);							\
		code_buf[code_idx++] = (ARM_INS_MOVW										\
						| (GTM_REG_CODEGEN_TEMP << ARM_SHIFT_RD						\
						| (offset & ARM_MASK_IMM4_HI) << ARM_SHIFT_IMM4_HI				\
						| (offset & ARM_MASK_IMM12 << ARM_SHIFT_IMM12)));				\
		if (MAX_16BIT < offset)												\
		{														\
			code_buf[code_idx++] = (ARM_INS_MOVT									\
							| (GTM_REG_CODEGEN_TEMP << ARM_SHIFT_RD)				\
							| (((offset >> 16) & ARM_MASK_IMM4_HI) << ARM_SHIFT_IMM4_HI)		\
							| (((offset >> 16) & ARM_MASK_IMM12) << ARM_SHIFT_IMM12));		\
		}														\
		/* Divide offset by 8, add 1, and multiply it by 8 -- multiply is done within the subtract */			\
		/* The ((offset / 8) + 1) * 8 is to ensure 8 byte stack alignment */						\
		code_buf[code_idx++] = (ARM_INS_LSR										\
						| GTM_REG_CODEGEN_TEMP << ARM_SHIFT_RM						\
						| GTM_REG_CODEGEN_TEMP << ARM_SHIFT_RD						\
						| 3 << ARM_SHIFT_IMM5);								\
		code_buf[code_idx++] = (ARM_INS_ADD_IMM										\
						| GTM_REG_CODEGEN_TEMP << ARM_SHIFT_RN						\
						| GTM_REG_CODEGEN_TEMP << ARM_SHIFT_RD						\
						| 1 << ARM_SHIFT_IMM12);							\
		code_buf[code_idx++] = (ARM_INS_SUB_REG										\
						| (GTM_REG_CODEGEN_TEMP << ARM_SHIFT_RM)					\
						| (ARM_REG_SP << ARM_SHIFT_RN)							\
						| (ARM_REG_SP << ARM_SHIFT_RD)							\
						| (ARM_SHIFT_TYPE_LSL << ARM_SHIFT_TYPE)					\
						| (3 << ARM_SHIFT_IMM5));							\
	}															\
	code_buf[code_idx++] = (ARM_INS_STR											\
					| ARM_U_BIT_ON										\
					| (offset << ARM_SHIFT_IMM12)								\
					| (ARM_REG_SP << ARM_SHIFT_RN)								\
					| (reg << ARM_SHIFT_RT));								\
}
#else	/* __armv6l__ */
#define GEN_STORE_ARG(reg, offset)												\
{																\
	if (((CGP_APPROX_ADDR == cg_phase || CGP_ADDR_OPT == cg_phase) && (MACHINE_REG_ARGS == vax_pushes_seen))		\
	    || ((CGP_ASSEMBLY == cg_phase || CGP_MACHINE == cg_phase) && (0 == vax_pushes_seen)))				\
	{															\
		code_buf[code_idx++] = (ARM_INS_MOV_REG										\
						| ARM_REG_SP << ARM_SHIFT_RM							\
						| ARM_REG_FP << ARM_SHIFT_RD);							\
		code_buf[code_idx++] = (ARM_INS_LDR										\
						| (ARM_REG_PC << ARM_SHIFT_RN)							\
						| (GTM_REG_CODEGEN_TEMP << ARM_SHIFT_RT));					\
		code_buf[code_idx++] = (ARM_INS_B);										\
		code_buf[code_idx++] = offset;											\
		/* Divide offset by 8, add 1, and multiply it by 8 -- multiply is done within the subtract */			\
		/* The ((offset / 8) + 1) * 8 is to ensure 8 byte stack alignment */						\
		code_buf[code_idx++] = (ARM_INS_LSR										\
						| GTM_REG_CODEGEN_TEMP << ARM_SHIFT_RM						\
						| GTM_REG_CODEGEN_TEMP << ARM_SHIFT_RD						\
						| 3 << ARM_SHIFT_IMM5);								\
		code_buf[code_idx++] = (ARM_INS_ADD_IMM										\
						| GTM_REG_CODEGEN_TEMP << ARM_SHIFT_RN						\
						| GTM_REG_CODEGEN_TEMP << ARM_SHIFT_RD						\
						| 1 << ARM_SHIFT_IMM12);							\
		code_buf[code_idx++] = (ARM_INS_SUB_REG										\
						| (GTM_REG_CODEGEN_TEMP << ARM_SHIFT_RM)					\
						| (ARM_REG_SP << ARM_SHIFT_RN)							\
						| (ARM_REG_SP << ARM_SHIFT_RD)							\
						| (ARM_SHIFT_TYPE_LSL << ARM_SHIFT_TYPE)					\
						| (3 << ARM_SHIFT_IMM5));							\
	}															\
	code_buf[code_idx++] = (ARM_INS_STR											\
					| ARM_U_BIT_ON										\
					| (offset << ARM_SHIFT_IMM12)								\
					| (ARM_REG_SP << ARM_SHIFT_RN)								\
					| (reg << ARM_SHIFT_RT));								\
}
#endif

#define GEN_MVAL_COPY(src_reg, trg_reg, size)											\
{																\
	for (words_to_move = size / SIZEOF(UINTPTR_T); D_REG_COUNT <= words_to_move; words_to_move -= D_REG_COUNT)		\
        {															\
		code_buf[code_idx++] = (ARM_INS_VLDMIA										\
						| src_reg << ARM_SHIFT_RN							\
						| D_REG_COUNT << ARM_SHIFT_IMM8);						\
		code_buf[code_idx++] = (ARM_INS_VSTMIA										\
						| trg_reg << ARM_SHIFT_RN							\
						| D_REG_COUNT << ARM_SHIFT_IMM8);						\
	}															\
	if (0 < words_to_move)													\
	{															\
		code_buf[code_idx++] = (ARM_INS_VLDMIA										\
						| src_reg << ARM_SHIFT_RN							\
						| words_to_move << ARM_SHIFT_IMM8);						\
		code_buf[code_idx++] = (ARM_INS_VSTMIA										\
						| trg_reg << ARM_SHIFT_RN							\
						| words_to_move << ARM_SHIFT_IMM8);						\
	}															\
}

#define GEN_PCREL														\
	code_buf[code_idx++] = (ARM_INS_MOV_REG											\
					| ARM_REG_PC << ARM_SHIFT_RM)								\
					| GTM_REG_CODEGEN_TEMP << ARM_SHIFT_RD							\

#define GEN_MOVE_REG(trg, src)													\
	code_buf[code_idx++] = (ARM_INS_MOV_REG											\
					| src << ARM_SHIFT_RM									\
					| trg << ARM_SHIFT_RD)

/*	CALL_INST_SIZE is the byte length of the minimum-length instruction
 *	sequence to implement a transfer table call.  In the case of ARM it is
 *	the sequence:
 *
 *		(ldr	r7, =xfer_table)
 *	; if offset < 4096
 *		ldr	r12, [r7, #offset]
 *		blx	r12			; call location in xfer_table
 *
 *	; if offset >= 4096 -- armv7l
 *		movw	r12, offset & 0xffff
 *		movt	r12, offset >> 16
 *		add	r12, r7
 *		ldr	r12, [r12]
 *		blx	r12			; call location in xfer_table
 *
 *	; if offset >= 4096 -- armv6l
 *		ldr	r12, =xxxx	(ldr	r12, [pc])
 *		b	yyy		(b	pc)
 *	xxxx	equ	offset
 *	yyy:
 *		add	r12, r7
 *		ldr	r12, [r12]
 *		blx	r12			; call location in xfer_table
 *
 *	This value is used to determine how to adjust the offset value for a
 *	relative call.
 */

#define CALL_INST_SIZE (2 * INST_SIZE)

#define GEN_XFER_TBL_CALL(xfer)													\
{																\
	GEN_LOAD_WORD(GTM_REG_CODEGEN_TEMP, GTM_REG_XFER_TABLE, xfer);								\
	code_buf[code_idx++] = ARM_INS_BLX | GTM_REG_CODEGEN_TEMP << ARM_SHIFT_RM;						\
	if (MACHINE_REG_ARGS < vax_pushes_seen)											\
	{															\
		code_buf[code_idx++] = (ARM_INS_MOV_REG										\
						| ARM_REG_FP << ARM_SHIFT_RM							\
						| ARM_REG_SP << ARM_SHIFT_RD);							\
	}															\
}

#ifdef __armv7l__
#define GEN_CMP_REG_IMM(reg, imm)												\
{ 																\
	if (imm < 0)														\
	{															\
		if (0 <= encode_immed12(-1 * imm))										\
		{														\
			code_buf[code_idx++] = (ARM_INS_CMN_IMM									\
							| ((reg & ARM_MASK_REG) << ARM_SHIFT_RN)				\
							| (encode_immed12(-1 * imm) << ARM_SHIFT_IMM12));			\
		} else														\
		{														\
			code_buf[code_idx++] = (ARM_INS_MOVW									\
							| (GTM_REG_CODEGEN_TEMP << ARM_SHIFT_RD)				\
							| ((-1 * imm & ARM_MASK_IMM4_HI) << ARM_SHIFT_IMM4_HI)			\
							| ((-1 * imm & ARM_MASK_IMM12) << ARM_SHIFT_IMM12));			\
			if (MAX_16BIT < (-1 * imm))										\
			{													\
				code_buf[code_idx++] = (ARM_INS_MOVT								\
								| (GTM_REG_CODEGEN_TEMP << ARM_SHIFT_RD)			\
								| (((-1 * imm >> 16) & ARM_MASK_IMM4_HI) << ARM_SHIFT_IMM4_HI)	\
								| (((-1 * imm >> 16) & ARM_MASK_IMM12) << ARM_SHIFT_IMM12));	\
			}													\
			code_buf[code_idx++] = (ARM_INS_CMN_REG									\
							| (reg << ARM_SHIFT_RN)							\
							| (GTM_REG_CODEGEN_TEMP << ARM_SHIFT_RM));				\
		}														\
	} else															\
	{															\
		if (0 <= encode_immed12(imm))											\
		{														\
			code_buf[code_idx++] = (ARM_INS_CMP_IMM									\
							| ((reg & ARM_MASK_REG) << ARM_SHIFT_RN)				\
							| (encode_immed12(imm) << ARM_SHIFT_IMM12));				\
		} else														\
		{														\
			code_buf[code_idx++] = (ARM_INS_MOVW									\
							| (GTM_REG_CODEGEN_TEMP << ARM_SHIFT_RD)				\
							| ((imm & ARM_MASK_IMM4_HI) << ARM_SHIFT_IMM4_HI)			\
							| ((imm & ARM_MASK_IMM12) << ARM_SHIFT_IMM12));				\
			if (MAX_16BIT < imm)											\
			{													\
				code_buf[code_idx++] = (ARM_INS_MOVT								\
								| (GTM_REG_CODEGEN_TEMP << ARM_SHIFT_RD)			\
								| (((imm >> 16) & ARM_MASK_IMM4_HI) << ARM_SHIFT_IMM4_HI)	\
								| (((imm >> 16) & ARM_MASK_IMM12) << ARM_SHIFT_IMM12));		\
			}													\
			code_buf[code_idx++] = (ARM_INS_CMP_REG									\
							| (reg << ARM_SHIFT_RN)							\
							| (GTM_REG_CODEGEN_TEMP << ARM_SHIFT_RM));				\
		}														\
	}															\
}
#else	/* __armv6l__ */
#define GEN_CMP_REG_IMM(reg, imm)												\
{ 																\
	if (imm < 0)														\
	{															\
		if (0 <= encode_immed12(-1 * imm))										\
		{														\
			code_buf[code_idx++] = (ARM_INS_CMN_IMM									\
							| ((reg & ARM_MASK_REG) << ARM_SHIFT_RN)				\
							| (encode_immed12(-1 * imm) << ARM_SHIFT_IMM12));			\
		} else														\
		{														\
			code_buf[code_idx++] = (ARM_INS_LDR									\
							| (ARM_REG_PC << ARM_SHIFT_RN)						\
							| (GTM_REG_CODEGEN_TEMP << ARM_SHIFT_RT));				\
			code_buf[code_idx++] = (ARM_INS_B);									\
			code_buf[code_idx++] = imm;										\
			code_buf[code_idx++] = (ARM_INS_CMN_REG									\
							| (reg << ARM_SHIFT_RN)							\
							| (GTM_REG_CODEGEN_TEMP << ARM_SHIFT_RM));				\
		}														\
	} else															\
	{															\
		if (0 <= encode_immed12(imm))											\
		{														\
			code_buf[code_idx++] = (ARM_INS_CMP_IMM									\
							| ((reg & ARM_MASK_REG) << ARM_SHIFT_RN)				\
							| (encode_immed12(imm) << ARM_SHIFT_IMM12));				\
		} else														\
		{														\
			code_buf[code_idx++] = (ARM_INS_LDR									\
							| (ARM_REG_PC << ARM_SHIFT_RN)						\
							| (GTM_REG_CODEGEN_TEMP << ARM_SHIFT_RT));				\
			code_buf[code_idx++] = (ARM_INS_B);									\
			code_buf[code_idx++] = imm;										\
			code_buf[code_idx++] = (ARM_INS_CMP_REG									\
							| (reg << ARM_SHIFT_RN)							\
							| (GTM_REG_CODEGEN_TEMP << ARM_SHIFT_RM));				\
		}														\
	}															\
}
#endif

/* Macros to return an instruction value. This is typcically used to modify an instruction
   that is already in the instruction buffer such as the last instruction that was created
   by emit_pcrel().
*/
#define IGEN_COND_BRANCH_REG_OFFSET(opcode, reg, disp)										\
					(opcode | ((disp & ARM_MASK_DISP) << ARM_SHIFT_BRANCH_DISP))
#define IGEN_UCOND_BRANCH_REG_OFFSET(opcode, reg, disp) IGEN_COND_BRANCH_REG_OFFSET(opcode, reg, disp)
#define IGEN_LOAD_ADDR_REG(reg)		(ARM_INS_SUB | ((reg & ARM_MASK_REG) << ARM_SHIFT_RD))
#define IGEN_LOAD_WORD_REG(reg)		(ARM_INS_LDR | (reg & ARM_MASK_REG) << ARM_SHIFT_RT)
#define IGEN_LOAD_NATIVE_REG(reg)	IGEN_LOAD_WORD_REG(reg)
#define IGEN_COND_BRANCH_OFFSET(disp)	((disp & ARM_MASK_BRANCH_DISP) << ARM_SHIFT_BRANCH_DISP)
#define IGEN_LOAD_LINKAGE(reg)		(ARM_INS_SUB | ((reg & ARM_MASK_REG) << ARM_SHIFT_RT))
#define IGEN_GENERIC_REG(opcode, reg)	(opcode | ((reg & ARM_MASK_REG) << ARM_SHIFT_RD))	/* rd and rt are at same place */

/* Some macros that are used in certain routines in emit_code.c. The names of
 * these macros start with the routine name they are used in.
*/

/* Branch has origin of +2 instructions. However, if the branch was nullified
 * in an earlier shrink_trips, the origin is the current instruction itself
*/
#define EMIT_JMP_ADJUST_BRANCH_OFFSET												\
	branch_offset = ((branch_offset != 0) ? branch_offset - 2 : -1)

/* Can jump be done within range of immediate operand */
/* Multiples of 4 in the range -33554432 (0xfe000000) to 33554428 (0x1fffffc) (from ARM v7 Architecture Reference Manual) */
#define EMIT_JMP_SHORT_CODE_CHECK												\
	(((branch_offset * INST_SIZE) >= -33554432) && ((branch_offset * INST_SIZE) <= 33554428))

/* Emit the short jump */
#define EMIT_JMP_SHORT_CODE_GEN													\
{																\
	code_buf[code_idx++] = (branchop											\
				| ((branch_offset & ARM_MASK_BRANCH_DISP) << ARM_SHIFT_BRANCH_DISP));				\
}

/* Is this a conditional branch? */
#define EMIT_JMP_OPPOSITE_BR_CHECK												\
		((branchop != ARM_INS_B) && (branchop != ARM_INS_BEQ))
#define EMIT_JMP_GEN_COMPARE

#define EMIT_JMP_LONG_CODE_CHECK	FALSE

#define EMIT_TRIP_ILIT_GEN		GEN_LOAD_IMMED(trg_reg, immediate)

/*
 * GT.M on AIX and SPARC is 64bit
 * By default the loads/stores use ldd/std(load double),
 * but if the value being dealt with is a word,the
 * opcode in generic_inst is changed to ldw/stw
 */
#define REVERT_GENERICINST_TO_WORD(inst)

#endif
