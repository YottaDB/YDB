/****************************************************************
 *								*
 * Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 * Copyright (c) 2017 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 * Copyright (c) 2017 Stephen L Johnson. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*	arm.h - ARM machine instruction information.
 *
 *	Requires "arm_registers.h" and "arm_gtm_registers.h".
 *
 */


/*	Machine instruction templates.  */

#define	ARM_INS_ADD_IMM		((unsigned)0xe28 << ARM_SHIFT_OP)
#define	ARM_INS_ADD_REG		((unsigned)0xe08 << ARM_SHIFT_OP)
#define ARM_INS_SUB_IMM		((unsigned)0xe24 << ARM_SHIFT_OP)
#define ARM_INS_SUB_REG		((unsigned)0xe04 << ARM_SHIFT_OP)
#define ARM_INS_SUB		((unsigned)0xe00 << ARM_SHIFT_OP)
#define ARM_INS_MOV_IMM		((unsigned)0xe3a << ARM_SHIFT_OP)
#define ARM_INS_MOV_REG		((unsigned)0xe1a << ARM_SHIFT_OP)
#define	ARM_INS_MOVW		((unsigned)0xe30 << ARM_SHIFT_OP)
#define	ARM_INS_MOVT		((unsigned)0xe34 << ARM_SHIFT_OP)
#define	ARM_INS_MVN		((unsigned)0xe3e << ARM_SHIFT_OP)
#define ARM_INS_LDR		((unsigned)0xe51 << ARM_SHIFT_OP)
#define ARM_INS_STR		((unsigned)0xe50 << ARM_SHIFT_OP)
#define ARM_INS_LSR		((unsigned)0xe1a << ARM_SHIFT_OP | 0x00020)
#define ARM_INS_POP		((unsigned)0xe8b << ARM_SHIFT_OP | 0xd0000)
#define ARM_INS_PUSH		((unsigned)0xe92 << ARM_SHIFT_OP | 0xd0000)

#define ARM_INS_PLD		((unsigned)0xf5d << ARM_SHIFT_OP | 0x0f000)
#define ARM_INS_VLDMIA		((unsigned)0xecb << ARM_SHIFT_OP | 0x00a00)
#define ARM_INS_VSTMIA		((unsigned)0xeca << ARM_SHIFT_OP | 0x00a00)

#define ARM_INS_BLX		((unsigned)0xe12 << ARM_SHIFT_OP | 0xfff30)
#define ARM_INS_BX		((unsigned)0xe12 << ARM_SHIFT_OP | 0xfff10)
#define ARM_INS_BL		((unsigned)0xeb << ARM_SHIFT_BRANCH)
#define ARM_INS_B		((unsigned)0xea << ARM_SHIFT_BRANCH)
#define ARM_INS_BEQ		((unsigned)0xa << ARM_SHIFT_BRANCH | (ARM_COND_EQ << ARM_SHIFT_COND))
#define ARM_INS_BGE		((unsigned)0xa << ARM_SHIFT_BRANCH | (ARM_COND_GE << ARM_SHIFT_COND))
#define ARM_INS_BGT		((unsigned)0xa << ARM_SHIFT_BRANCH | (ARM_COND_GT << ARM_SHIFT_COND))
#define ARM_INS_BLE		((unsigned)0xa << ARM_SHIFT_BRANCH | (ARM_COND_LE << ARM_SHIFT_COND))
#define ARM_INS_BLT		((unsigned)0xa << ARM_SHIFT_BRANCH | (ARM_COND_LT << ARM_SHIFT_COND))
#define ARM_INS_BNE		((unsigned)0xa << ARM_SHIFT_BRANCH | (ARM_COND_NE << ARM_SHIFT_COND))
#define ARM_INS_TEQ		((unsigned)0xe13 << ARM_SHIFT_OP)
#define ARM_INS_TST		((unsigned)0xe31 << ARM_SHIFT_OP)
#define ARM_INS_CMN_IMM		((unsigned)0xe37 << ARM_SHIFT_OP)
#define ARM_INS_CMN_REG		((unsigned)0xe17 << ARM_SHIFT_OP)
#define ARM_INS_CMP_IMM		((unsigned)0xe35 << ARM_SHIFT_OP)
#define ARM_INS_CMP_REG		((unsigned)0xe15 << ARM_SHIFT_OP)
#define ARM_INS_ORRS		((unsigned)0xe19 << ARM_SHIFT_OP)
#define ARM_INS_ANDS		((unsigned)0xe21 << ARM_SHIFT_OP)
#define ARM_INS_NOP		((unsigned)0xe32 << ARM_SHIFT_OP | 0x0f000)

#define ARM_REG2IMM_BIT		((unsigned)0x020 << ARM_SHIFT_OP)	/* Converts ADD/SUB register to ADD/SUB immediate */

#define ARM_INS_BIS		((unsigned)0xc << ARM_SHIFT_DP | ARM_COND_ALWAYS << ARM_SHIFT_COND)
#define ARM_INS_BLBS		((unsigned)0x3c << ARM_SHIFT_OP)

/* Branch to Subroutine */
#define ARM_INS_BSR		((unsigned)0x34 << ARM_SHIFT_OP)
/* Jump */
#define	ARM_INS_JMP		((unsigned)0x1a << ARM_SHIFT_OP)

#define	ARM_INS_JSR		((unsigned)0x1a << ARM_SHIFT_OP | 1 << ARM_SHIFT_BRANCH_FUNC)
/* Load Sign-Extended Longword from Memory to Register */
#define	ARM_INS_LDL		((unsigned)0x01 << ARM_SHIFT_OP)
/* Load Quadword from Memory to Register */
#define	ARM_INS_LDQ		((unsigned)0x29 << ARM_SHIFT_OP)
#define	ARM_INS_RET		((unsigned)0x1a << ARM_SHIFT_OP | 2 << ARM_SHIFT_BRANCH_FUNC)
/* Store Longword from Register to Memory */
#define	ARM_INS_STL		((unsigned)0x2c << ARM_SHIFT_OP)
/* Store Quadword from Register to Memory */
#define	ARM_INS_STQ		((unsigned)0x2d << ARM_SHIFT_OP)
/* Subtract Longword */
#define	ARM_INS_SUBL		((unsigned)0x2 << ARM_SHIFT_DP)
/* Subtract Quadword */
#define	ARM_INS_SUBQ		((unsigned)0x10 << ARM_SHIFT_OP | 0x29 << ARM_SHIFT_FUNC)

#define ARM_SHIFT_TYPE_LSL	((unsigned)0x00 << ARM_SHIFT_TYPE)
#define ARM_INS_DP_IMMED	((unsigned)0x1 << ARM_SHIFT_I_BIT)

/*	Bit definitions for registers (used in push and pop) */
#define ARM_R0_BIT		((unsigned)0x0001)
#define ARM_R1_BIT		((unsigned)0x0002)
#define ARM_R2_BIT		((unsigned)0x0004)
#define ARM_R3_BIT		((unsigned)0x0008)
#define ARM_R4_BIT		((unsigned)0x0010)
#define ARM_R5_BIT		((unsigned)0x0020)
#define ARM_R6_BIT		((unsigned)0x0040)
#define ARM_R7_BIT		((unsigned)0x0080)
#define ARM_R8_BIT		((unsigned)0x0100)
#define ARM_R9_BIT		((unsigned)0x0200)
#define ARM_R10_BIT		((unsigned)0x0400)
#define ARM_R11_BIT		((unsigned)0x0800)
#define ARM_FP_BIT		((unsigned)0x0800)
#define ARM_R12_BIT		((unsigned)0x1000)
#define ARM_R13_BIT		((unsigned)0x2000)
#define ARM_R14_BIT		((unsigned)0x4000)
#define ARM_R15_BIT		((unsigned)0x8000)

/*	Bit offsets to instruction fields.  */

#define ARM_SHIFT_COND		28
#define ARM_SHIFT_DP		21	/* data processing instruction shift */
#define ARM_SHIFT_OP		20
#define ARM_SHIFT_BRANCH	24

#define ARM_SHIFT_RN		16
#define	ARM_SHIFT_RD		12
#define	ARM_SHIFT_RT		12
#define	ARM_SHIFT_RM		0
#define ARM_SHIFT_BRANCH_DISP	0
#define ARM_SHIFT_DISP		0
#define ARM_SHIFT_TYPE		5

#define ARM_SHIFT_IMM4		16
#define ARM_SHIFT_IMM4_HI	4		/* Only shift the high nibble */
#define ARM_SHIFT_IMM5		7
#define ARM_SHIFT_IMM8		0
#define ARM_SHIFT_IMM12		0
#define ARM_SHIFT_P_BIT		24
#define ARM_SHIFT_U_BIT		23
#define ARM_SHIFT_B_BIT		22
#define ARM_SHIFT_W_BIT		21
#define ARM_SHIFT_S_BIT		20

/*	Bit masks for instruction fields.  */

#define CLEAR_RN_REG		~(0xf << ARM_SHIFT_RN)
#define ARM_MASK_BRANCH_DISP	0xffffff
#define	ARM_MASK_BRANCH_FUNC	0xc00000
#define ARM_MASK_DISP		0xffffff
#define	ARM_MASK_OP		0xff		/* opcode */
#define	ARM_MASK_REG		0xf		/* register */
#define ARM_MASK_COND		0xf		/* conditional */
#define ARM_MASK_IMMED_HI	0xf0		/* high nibble of immed */
#define ARM_MASK_IMMED_LO	0x0f		/* low nibble of immed */
#define ARM_MASK_SHIFT_TYPE	0x3

#define ARM_MASK_IMM4_HI	0xf000		/* high nibble of 16 bit immed */
#define ARM_MASK_IMM4		0xf
#define ARM_MASK_IMM5		0x1f
#define ARM_MASK_IMM8		0xff
#define ARM_MASK_IMM12		0x0fff


#define ARM_U_BIT_ON		(1 << ARM_SHIFT_U_BIT)
#define ARM_U_BIT_OFF		~(1 << ARM_SHIFT_U_BIT)
#define ARM_P_BIT_ON		(1 << ARM_SHIFT_P_BIT)
#define ARM_W_BIT_ON		(1 << ARM_SHIFT_W_BIT)
#define ARM_B_BIT_ON		(1 << ARM_SHIFT_B_BIT)
#define ARM_S_BIT_ON		(1 << ARM_SHIFT_S_BIT)
#define ARM_I_BIT_OFF		0

#define ARM_COND_EQ		0x0
#define ARM_COND_NE		0x1
#define ARM_COND_GE		0xa
#define ARM_COND_LT		0xb
#define ARM_COND_GT		0xc
#define ARM_COND_LE		0xd
#define ARM_COND_ALWAYS		0xe

#define ARM_MASK_COND		0xf
#define ARM_MASK_WHOLE_OP	0xf7f
#define ARM_MASK_RN		0xf
#define ARM_MASK_RD		0xf
#define ARM_MASK_RT		0xf
#define ARM_MASK_SHIFT_AMT	0x1f
#define ARM_MASK_SHIFT		0x3
#define ARM_MASK_RM		0xf
#define ARM_MASK_RS		0xf
#define ARM_MASK_ROTATE		0xf
#define ARM_MASK_IMMEDIATE	0xff
#define ARM_MASK_24_BIT_OFF	0xffffff
#define ARM_MASK_REGISTERS	0xffff

#define U_BIT_SET(ains)		(1 == ((ains & ARM_U_BIT_ON) >> ARM_SHIFT_U_BIT))

#ifdef DEBUG
#define GET_OPCODE(ains)	((ains >> ARM_SHIFT_OP) & ARM_MASK_OP)
#define GET_RT(ains)		((ains >> ARM_SHIFT_RT) & ARM_MASK_REG)
#define GET_RD(ains)		((ains >> ARM_SHIFT_RD) & ARM_MASK_REG)
#define GET_RN(ains)		((ains >> ARM_SHIFT_RN) & ARM_MASK_REG)
#define GET_RM(ains)		((ains >> ARM_SHIFT_RM) & ARM_MASK_REG)
#define GET_MEMDISP(ains)	((ains >> ARM_SHIFT_DISP) & ARM_MASK_DISP)
#define GET_REGISTERS(ains)	(ains & ARM_MASK_REGISTERS)
#define GET_IMM5(ains)		((ains >> ARM_SHIFT_IMM5) & ARM_MASK_IMM5)
#define GET_IMM8(ains)		((ains >> ARM_SHIFT_IMM8) & ARM_MASK_IMM8)
#define GET_IMM12(ains)		((ains >> ARM_SHIFT_IMM12) & ARM_MASK_IMM12)
#define GET_IMM16(ains)		((((ains >> ARM_SHIFT_IMM4) & ARM_MASK_IMM4) << ARM_SHIFT_RD) | (ains >> ARM_SHIFT_IMM12) & ARM_MASK_IMM12)
#define GET_BRDISP(ains)	((ains >> ARM_SHIFT_BRANCH_DISP) & ARM_MASK_BRANCH_DISP)
#define GET_FUNC(ains)		((ains >> ARM_SHIFT_FUNC) & ARM_MASK_FUNC)
#define GET_COND(ains)		((ains >> ARM_SHIFT_COND) & ARM_MASK_COND)

#define ADD_INST	"add"
#define SUB_INST	"sub"
#define SUBS_INST	"subs"
#define MOV_INST	"mov"
#define MOVW_INST	"movw"
#define MOVT_INST	"movt"
#define MVN_INST	"mvn"
#define LDR_INST	"ldr"
#define STR_INST	"str"
#define LSR_INST	"lsr"
#define BLX_INST	"blx"
#define BX_INST		"bx"
#define CMN_INST	"cmn"
#define CMP_INST	"cmp"
#define TST_INST	"tst"
#define BL_INST		"bl"
#define B_INST		"b"
#define ORRS_INST	"orrs"
#define NOP_INST	"nop"
#define POP_INST	"pop"
#define PUSH_INST	"push"
#define PLD_INST	"pld"
#define VSTMIA_INST	"vstmia"
#define VLDMIA_INST	"vldmia"
#define LSL_SHIFT_TYPE	"lsl"
#define BEQ_COND	"beq"
#define BNE_COND	"bne"
#define BGE_COND	"bge"
#define BLT_COND	"blt"
#define BGT_COND	"bgt"
#define BLE_COND	"ble"
#define INV_INST	"<invalid instruction>"

/* Space for op_code to be in */
#define OPSPC		7
#endif
