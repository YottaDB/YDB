/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*	axp.h - AXP machine instruction information.
 *
 *	Requires "axp_registers.h" and "axp_gtm_registers.h".
 *
 */


/*	Machine instruction templates.  */

#define	ALPHA_INS_ADDL		((unsigned)0x10 << ALPHA_SHIFT_OP)
#define ALPHA_INS_BEQ		((unsigned)0x39 << ALPHA_SHIFT_OP)
#define ALPHA_INS_BGE		((unsigned)0x3e << ALPHA_SHIFT_OP)
#define ALPHA_INS_BGT		((unsigned)0x3f << ALPHA_SHIFT_OP)
#define	ALPHA_INS_BIS		((unsigned)0x11 << ALPHA_SHIFT_OP | 0x20 << ALPHA_SHIFT_FUNC)
#define ALPHA_INS_BLE		((unsigned)0x3b << ALPHA_SHIFT_OP)
#define ALPHA_INS_BLT		((unsigned)0x3a << ALPHA_SHIFT_OP)
#define ALPHA_INS_BLBC		((unsigned)0x38 << ALPHA_SHIFT_OP)
#define ALPHA_INS_BLBS		((unsigned)0x3c << ALPHA_SHIFT_OP)
#define ALPHA_INS_BNE		((unsigned)0x3d << ALPHA_SHIFT_OP)
#define ALPHA_INS_BSR		((unsigned)0x34 << ALPHA_SHIFT_OP)
#define ALPHA_INS_BR		((unsigned)0x30 << ALPHA_SHIFT_OP)
#define	ALPHA_INS_JMP		((unsigned)0x1a << ALPHA_SHIFT_OP)
#define	ALPHA_INS_JSR		((unsigned)0x1a << ALPHA_SHIFT_OP | 1 << ALPHA_SHIFT_BRANCH_FUNC)
#define	ALPHA_INS_LDA		((unsigned)0x08 << ALPHA_SHIFT_OP)
#define	ALPHA_INS_LDAH		((unsigned)0x09 << ALPHA_SHIFT_OP)
#define	ALPHA_INS_LDL		((unsigned)0x28 << ALPHA_SHIFT_OP)
#define	ALPHA_INS_LDQ		((unsigned)0x29 << ALPHA_SHIFT_OP)
#define	ALPHA_INS_RET		((unsigned)0x1a << ALPHA_SHIFT_OP | 2 << ALPHA_SHIFT_BRANCH_FUNC)
#define	ALPHA_INS_STL		((unsigned)0x2c << ALPHA_SHIFT_OP)
#define	ALPHA_INS_STQ		((unsigned)0x2d << ALPHA_SHIFT_OP)
#define	ALPHA_INS_SUBL		((unsigned)0x10 << ALPHA_SHIFT_OP | 0x9 << ALPHA_SHIFT_FUNC)
#define	ALPHA_INS_SUBQ		((unsigned)0x10 << ALPHA_SHIFT_OP | 0x29 << ALPHA_SHIFT_FUNC)


/*	Bit offsets to instruction fields.  */

#define ALPHA_SHIFT_OP		26
#define	ALPHA_SHIFT_BRANCH_FUNC	14
#define	ALPHA_SHIFT_FUNC	5
#define	ALPHA_SHIFT_LITERAL	13
#define ALPHA_SHIFT_RA		21
#define	ALPHA_SHIFT_RB		16
#define	ALPHA_SHIFT_RC		0
#define ALPHA_SHIFT_BRANCH_DISP	0
#define ALPHA_SHIFT_DISP	0


/*	Bit masks for instruction fields.  */

#define	ALPHA_BIT_LITERAL	(1 << 12)
#define ALPHA_MASK_BRANCH_DISP	0x1fffff
#define	ALPHA_MASK_BRANCH_FUNC	0xc00000
#define ALPHA_MASK_DISP		0xffff
#define	ALPHA_MASK_FUNC		0x7f
#define	ALPHA_MASK_LITERAL	0xff
#define	ALPHA_MASK_OP		0x3f
#define	ALPHA_MASK_REG		0x1f


/*	Alternative assembler mnemonics for machine instruction.  */

#define	ALPHA_INS_CLRQ	(ALPHA_INS_BIS \
				| (ALPHA_REG_ZERO << ALPHA_SHIFT_RA) \
				| (ALPHA_REG_ZERO << ALPHA_SHIFT_RB))
#define ALPHA_INS_LPC	(ALPHA_INS_BR \
				| (GTM_REG_CODEGEN_TEMP << ALPHA_SHIFT_RA))
#define	ALPHA_INS_MOVE	(ALPHA_INS_BIS \
				| ALPHA_REG_ZERO << ALPHA_SHIFT_RB)
#define ALPHA_INS_NOP	(ALPHA_INS_BIS \
				| (ALPHA_REG_ZERO << ALPHA_SHIFT_RA) \
				| (ALPHA_REG_ZERO << ALPHA_SHIFT_RB) \
				| (ALPHA_REG_ZERO << ALPHA_SHIFT_RC))


/*	Construction forms.  */

#define ALPHA_BRA(op,ra,disp)	((op) | ((ra) << ALPHA_SHIFT_RA) | (disp)&ALPHA_MASK_BRANCH_DISP)
#define ALPHA_JMP(op,ra,rb)	((op) | ((ra) << ALPHA_SHIFT_RA) | ((rb) << ALPHA_SHIFT_RB))
#define ALPHA_LIT(op,ra,lit,rc)	((op) | ((ra) << ALPHA_SHIFT_RA) \
					| (((lit)&ALPHA_MASK_LITERAL) << ALPHA_SHIFT_LITERAL) \
					| ALPHA_BIT_LITERAL \
					| ((rc) << ALPHA_SHIFT_RC))
#define ALPHA_MEM(op,ra,rb,disp)((op) | ((ra) << ALPHA_SHIFT_RA) | ((rb) << ALPHA_SHIFT_RB) | (disp)&ALPHA_MASK_DISP)
#define ALPHA_OPR(op,ra,rb,rc)	((op) | ((ra) << ALPHA_SHIFT_RA) | ((rb) << ALPHA_SHIFT_RB) | ((rc) << ALPHA_SHIFT_RC))

#ifdef DEBUG
#define GET_OPCODE(ains) ((ains >> ALPHA_SHIFT_OP) & ALPHA_MASK_OP)
#define GET_RA(ains) ((ains >> ALPHA_SHIFT_RA) & ALPHA_MASK_REG)
#define GET_RB(ains) ((ains >> ALPHA_SHIFT_RB) & ALPHA_MASK_REG)
#define GET_RC(ains) ((ains >> ALPHA_SHIFT_RC) & ALPHA_MASK_REG)
#define GET_MEMDISP(ains) ((ains >> ALPHA_SHIFT_DISP) & ALPHA_MASK_DISP)
#define GET_BRDISP(ains) ((ains >> ALPHA_SHIFT_BRANCH_DISP) & ALPHA_MASK_BRANCH_DISP)
#define GET_FUNC(ains) ((ains >> ALPHA_SHIFT_FUNC) & ALPHA_MASK_FUNC)

#define ADDL_INST	"addl"
#define SUBL_INST	"subl"
#define SUBQ_INST	"subq"
#define BIS_INST	"bis"
#define JSR_INST	"jsr"
#define RET_INST	"ret"
#define JMP_INST	"jmp"
#define LDA_INST	"lda"
#define LDAH_INST	"ldah"
#define LDL_INST	"ldl"
#define LDQ_INST	"ldq"
#define STL_INST	"stl"
#define STQ_INST	"stq"
#define BR_INST		"br"
#define BSR_INST	"bsr"
#define BLBC_INST	"blbc"
#define BEQ_INST	"beq"
#define BLT_INST	"blt"
#define BLE_INST	"ble"
#define BLBS_INST	"blbs"
#define BNE_INST	"bne"
#define BGE_INST	"bge"
#define BGT_INST	"bgt"
#define CONSTANT	"Constant 0x"

/* Space for op_code to be in */
#define OPSPC		7
#endif
