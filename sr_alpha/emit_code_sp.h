/****************************************************************
 *								*
 *	Copyright 2003, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef EMIT_CODE_SP_INCLUDED
#define EMIT_CODE_SP_INCLUDED

#include "axp_registers.h"
#include "axp_gtm_registers.h"
#include "axp.h"

void	emit_base_offset(int base, int offset);
int	alpha_adjusted_upper(int offset);

#ifdef DEBUG
void    format_machine_inst(void);
void	fmt_ra(void);
void	fmt_ra_rb(void);
void	fmt_ra_rb_rc(void);
void	fmt_ra_mem(void);
void	fmt_ra_brdisp(void);
#endif

#define INST_SIZE (int)SIZEOF(uint4)
#define BRANCH_OFFSET_FROM_IDX(idx_start, idx_end) (idx_end - (idx_start + 1))
#define LONG_JUMP_OFFSET (0x4ffffffc) /* should be large enough to force the long jump instruction sequence */
#define MAX_BRANCH_CODEGEN_SIZE 32  /* The length in bytes, of the longest form of branch instruction sequence */

#define MAX_OFFSET 			0x3fff
#define STACK_ARG_OFFSET(indx)		(8 * (indx)) /* All arguments on Alpha platforms are 8 bytes wide on stack */
#define MACHINE_FIRST_ARG_REG		ALPHA_REG_A0

/* Register usage in some of the code generation expansions */
#define CALLS_TINT_TEMP_REG		ALPHA_REG_R1
#define CLRL_REG			ALPHA_REG_ZERO
#define CMPL_TEMP_REG			ALPHA_REG_T1
#define GET_ARG_REG(indx)		(ALPHA_REG_A0 + (indx))
#define MOVC3_SRC_REG			ALPHA_REG_R0
#define MOVC3_TRG_REG			ALPHA_REG_R1
#define MOVL_RETVAL_REG			ALPHA_REG_V0
#define MOVL_REG_R1			ALPHA_REG_R1

/* Macros to define the opcodes for use in emit_jmp() and emit_tip() args */

#define GENERIC_OPCODE_BEQ		((uint4)ALPHA_INS_BEQ)
#define GENERIC_OPCODE_BGE		((uint4)ALPHA_INS_BGE)
#define GENERIC_OPCODE_BGT		((uint4)ALPHA_INS_BGT)
#define GENERIC_OPCODE_BLE		((uint4)ALPHA_INS_BLE)
#define GENERIC_OPCODE_BLT		((uint4)ALPHA_INS_BLT)
#define GENERIC_OPCODE_BNE		((uint4)ALPHA_INS_BNE)
#define GENERIC_OPCODE_BLBC		((uint4)ALPHA_INS_BLBC)
#define GENERIC_OPCODE_BLBS		((uint4)ALPHA_INS_BLBS)
#define GENERIC_OPCODE_BR		((uint4)ALPHA_INS_BR)
#define GENERIC_OPCODE_LDA		((uint4)ALPHA_INS_LDA)
#define GENERIC_OPCODE_LOAD		((uint4)ALPHA_INS_LDL)
#define GENERIC_OPCODE_STORE		((uint4)ALPHA_INS_STL)
#define GENERIC_OPCODE_NOP		((uint4)ALPHA_INS_NOP)

/* Macro to extract parts of generic opcodes */
#define GENXCT_LOAD_SRCREG(inst)	((inst >> ALPHA_SHIFT_RB) & ALPHA_MASK_REG)

/* Macros to create specific generated code sequences */

/* Note that the GEN_CLEAR/SET_TRUTH macros are only used on VMS (TRUTH_IN_REG) */
#define GEN_CLEAR_TRUTH			code_buf[code_idx++] = (ALPHA_INS_STL | ALPHA_REG_ZERO << ALPHA_SHIFT_RA \
                                                                | GTM_REG_DOLLAR_TRUTH << ALPHA_SHIFT_RB \
								| 0 << ALPHA_SHIFT_DISP)
#define GEN_SET_TRUTH                   { \
                                                code_buf[code_idx++] = (ALPHA_INS_BIS | ALPHA_REG_ZERO << ALPHA_SHIFT_RA \
                                                                        | 1 << ALPHA_SHIFT_LITERAL | ALPHA_BIT_LITERAL \
                                                                        | GTM_REG_CODEGEN_TEMP << ALPHA_SHIFT_RC); \
						code_buf[code_idx++] = (ALPHA_INS_STL | GTM_REG_CODEGEN_TEMP << ALPHA_SHIFT_RA \
                                                                        | GTM_REG_DOLLAR_TRUTH << ALPHA_SHIFT_RB \
                                                                        | 0 << ALPHA_SHIFT_DISP); \
                                        }
#define GEN_LOAD_ADDR(reg, breg, disp)	code_buf[code_idx++] = (ALPHA_INS_LDA | reg << ALPHA_SHIFT_RA \
								| breg << ALPHA_SHIFT_RB \
								| (disp & ALPHA_MASK_DISP) << ALPHA_SHIFT_DISP)
#define GEN_LOAD_WORD(reg, breg, disp)	code_buf[code_idx++] = (ALPHA_INS_LDL | reg << ALPHA_SHIFT_RA \
								| breg << ALPHA_SHIFT_RB \
								| (disp & ALPHA_MASK_DISP) << ALPHA_SHIFT_DISP)
#define GEN_STORE_WORD(reg, breg, disp)	code_buf[code_idx++] = (ALPHA_INS_STL | reg << ALPHA_SHIFT_RA \
								| breg << ALPHA_SHIFT_RB \
								| (disp & ALPHA_MASK_DISP) << ALPHA_SHIFT_DISP)
#define GEN_LOAD_IMMED(reg, disp)	GEN_LOAD_ADDR(reg, ALPHA_REG_ZERO, disp)
#define GEN_CLEAR_WORD_EMIT(reg)        emit_trip(*(fst_opr + *inst++), TRUE, ALPHA_INS_STL, reg)
#define GEN_LOAD_WORD_EMIT(reg)		emit_trip(*(fst_opr + *inst++), TRUE, ALPHA_INS_LDL, reg)
#define GEN_SUBTRACT_REGS(src1, src2, trgt) \
                                        code_buf[code_idx++] = (ALPHA_INS_SUBL \
								| src1 << ALPHA_SHIFT_RA \
								| src2 << ALPHA_SHIFT_RB \
								| trgt << ALPHA_SHIFT_RC)
#define GEN_ADD_IMMED(reg, imval)	code_buf[code_idx++] = (ALPHA_INS_ADDL \
								| reg << ALPHA_SHIFT_RA \
								| imval << ALPHA_SHIFT_LITERAL | ALPHA_BIT_LITERAL \
								| reg << ALPHA_SHIFT_RC)
#define GEN_JUMP_REG(reg)		code_buf[code_idx++] = (ALPHA_INS_JMP | ALPHA_REG_ZERO << ALPHA_SHIFT_RA \
								| reg << ALPHA_SHIFT_RB)
#define GEN_STORE_ARG(reg, offset)	code_buf[code_idx++] = (ALPHA_INS_STQ | reg << ALPHA_SHIFT_RA \
								| ALPHA_REG_SP << ALPHA_SHIFT_RB \
								| (offset & ALPHA_MASK_DISP) << ALPHA_SHIFT_DISP)
#define GEN_PCREL			code_buf[code_idx++] = (ALPHA_INS_LPC)
#define GEN_MOVE_REG(trg, src)		code_buf[code_idx++] = (ALPHA_INS_MOVE | src << ALPHA_SHIFT_RA | trg << ALPHA_SHIFT_RC)

#if	defined(__vms)
/*	CALL_INST_SIZE is the byte length of the minimum-length instruction sequence to implement a transfer
 *	table call.  In the case of OpenVMS AXP, this is the sequence:
 *
 *		ldl	r27, xfer(r11)		; get address of procedure descriptor from transfer table
 *		ldq	r26, 8(r27)		; get code address of procedure from procedure descriptor
 *		jmp	r26, (r26)		; call it
 *
 *	This value is used to determine how to adjust the offset value for a relative call and may not
 *	be appropriate for the Alpha because VAX relative calls are emulated on the Alpha differently.
*/
#  define CALL_INST_SIZE (3 * INST_SIZE)
#  define GEN_XFER_TBL_CALL(xfer)							\
   {											\
	emit_base_offset(GTM_REG_XFER_TABLE, xfer);					\
	code_buf[code_idx++] |= ALPHA_INS_LDL | ALPHA_REG_PV << ALPHA_SHIFT_RA;		\
	emit_base_offset(ALPHA_REG_PV, 8);						\
	code_buf[code_idx++] |= ALPHA_INS_LDQ | ALPHA_REG_RA << ALPHA_SHIFT_RA;		\
	code_buf[code_idx++] = ALPHA_INS_JSR | ALPHA_REG_RA << ALPHA_SHIFT_RA | ALPHA_REG_RA << ALPHA_SHIFT_RB; \
   }
#elif	defined(__osf__)
/*	CALL_INST_SIZE is the byte length of the minimum-length instruction sequence to implement a transfer
 *	table call.  In the case of OSF/1 (Digital Unix) AXP, this is the sequence:
 *
 *		ldl	r27, offset(r12)	# get address of entry point from transfer table
 *		jmp	r26, (r27)		# call it
 *
 *	This value is used to determine how to adjust the offset value for a relative call and may not
 *	be appropriate for the Alpha because VAX relative calls are emulated on the Alpha differently.
*/
#  define CALL_INST_SIZE (2 * INST_SIZE)
#  define GEN_XFER_TBL_CALL(xfer)							\
   {											\
	emit_base_offset(GTM_REG_XFER_TABLE, xfer);					\
	code_buf[code_idx++] |= ALPHA_INS_LDL | ALPHA_REG_PV << ALPHA_SHIFT_RA;		\
	code_buf[code_idx++] = ALPHA_INS_JSR | ALPHA_REG_RA << ALPHA_SHIFT_RA | ALPHA_REG_PV << ALPHA_SHIFT_RB; \
   }
#else
#	error "Unsupported platform"
#endif


/* Macros to return an instruction value. This is typcically used to modify an instruction
   that is already in the instruction buffer such as the last instruction that was created
   by emit_pcrel().
*/
#define IGEN_COND_BRANCH_REG_OFFSET(opcode, reg, disp) (opcode | ((reg & ALPHA_MASK_REG) << ALPHA_SHIFT_RA) \
					| ((disp & ALPHA_MASK_DISP) << ALPHA_SHIFT_BRANCH_DISP))
#define IGEN_UCOND_BRANCH_REG_OFFSET(opcode, reg, disp) IGEN_COND_BRANCH_REG_OFFSET(opcode, reg, disp)
#define IGEN_LOAD_ADDR_REG(reg)		(ALPHA_INS_LDA | (reg & ALPHA_MASK_REG) << ALPHA_SHIFT_RA)
#define IGEN_LOAD_WORD_REG(reg)		(ALPHA_INS_LDL | (reg & ALPHA_MASK_REG) << ALPHA_SHIFT_RA)
#define IGEN_LOAD_NATIVE_REG(reg)	IGEN_LOAD_WORD_REG(reg)
#define IGEN_COND_BRANCH_OFFSET(disp)	((disp & ALPHA_MASK_BRANCH_DISP) << ALPHA_SHIFT_BRANCH_DISP)
#define IGEN_UCOND_BRANCH_OFFSET(disp)	IGEN_COND_BRANCH_OFFSET(disp)
#define IGEN_LOAD_LINKAGE(reg)		(ALPHA_INS_LDQ | (reg & ALPHA_MASK_REG) << ALPHA_SHIFT_RA)
#define IGEN_GENERIC_REG(opcode, reg)	(opcode | ((reg & ALPHA_MASK_REG) << ALPHA_SHIFT_RA))

/* Some macros that are used in certain routines in emit_code.c. The names of these
   macros start with the routine name they are used in.
*/

/* Branch has origin of +1 instructions. However, if the branch was nullified in an earlier shrink_trips,
 * the origin is the current instruction itself */
#define EMIT_JMP_ADJUST_BRANCH_OFFSET	branch_offset = ((branch_offset != 0) ? branch_offset - 1 : 0)
/* Can jump be done within range of immediate operand */
#define EMIT_JMP_SHORT_CODE_CHECK	(branch_offset >= (-(ALPHA_MASK_BRANCH_DISP/2) - 1)	\
					 && branch_offset <= (ALPHA_MASK_BRANCH_DISP/2))
/* Emit the short jump */
#define EMIT_JMP_SHORT_CODE_GEN									\
{												\
	code_buf[code_idx++] = (branchop | (reg << ALPHA_SHIFT_RA)				\
		| ((branch_offset & ALPHA_MASK_BRANCH_DISP) << ALPHA_SHIFT_BRANCH_DISP));	\
	branch_offset--;									\
}
/* Is this a conditional branch? */
#define EMIT_JMP_OPPOSITE_BR_CHECK	(branchop != ALPHA_INS_BR && (branchop != ALPHA_INS_BEQ || reg != ALPHA_REG_ZERO))
#define EMIT_JMP_GEN_COMPARE		/* No compare necessary */
#define EMIT_JMP_LONG_CODE_CHECK	FALSE
/* Is the offset field in this instruction zero? */
#define EMIT_JMP_ZERO_DISP_COND		(0 == ((code_buf[code_idx] >> ALPHA_SHIFT_DISP) & ALPHA_MASK_DISP))
/* Emit code to load a given numeric literal */
#define EMIT_TRIP_ILIT_GEN		{	/* Emit liternal number */						\
	                                        emit_base_offset(ALPHA_REG_ZERO, immediate);				\
						code_buf[code_idx++] |= (ALPHA_INS_LDA |				\
									 (trg_reg & ALPHA_MASK_REG) << ALPHA_SHIFT_RA); \
                                        }
/*
 * GT.M on AIX and SPARC is 64bit
 * By default the loads/stores use ldd/std(load double),
 * but if the value being dealt with is a word,the
 * opcode in generic_inst is changed to ldw/stw
 */
#define REVERT_GENERICINST_TO_WORD(inst)

#endif
