/****************************************************************
 *                                                              *
 *      Copyright 2007, 2009 Fidelity Information Services, Inc *
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

#include "mdef.h"
#include "x86_64.h"

#ifdef DEBUG
void format_machine_inst(void);
void initialize_registers(void);
void reset_instruction(void);
void print_source_operand(void);
void print_destination_operand(void);
void print_instruction(void);
void set_memory_reg(void);
void set_register_reg(void);
void clear_memory_reg(void);
#endif

void emit_base_offset(int base, int offset);

/*  Define the COND values for use in the 2 byte JCC instructions.. */

#define INST_SIZE 	1
#define XFER_BYTE_INST_SIZE 3
#define XFER_LONG_INST_SIZE 6
#define BRB_INST_SIZE 2
#define JMP_LONG_INST_SIZE 5
#define CALL_4LCLDO_XFER 2	/* index in ttt from start of call[sp] and forlcldo to xfer_table index */
#define MAX_BRANCH_CODEGEN_SIZE 32  /* The length in bytes, of the longest form of branch instruction sequence */


int x86_64_arg_reg(int indx);
#define GET_ARG_REG(indx)	x86_64_arg_reg(indx)

/* Should be offset from RSP */
#define STACK_ARG_OFFSET(indx)	(8 * indx)

enum condition
{
	JO,
	JNO,
	JB,
	JC = JB,
	JNAE = JC,
	JNB,
	JNC = JNB,
	JAE = JNB,
	JZ,
	JE = JZ,
	JNZ,
	JNE = JNZ,
	JBE,
	JNA = JBE,
	JNBE,
	JA = JNBE,
	JS,
	JNS,
	JP,
	JPE = JP,
	JNP,
	JPO = JNP,
	JL,
	JNGE = JL,
	JNL,
	JGE = JNL,
	JLE,
	JNG = JLE,
	JNLE,
	JG = JNLE
};

#define REX_FIELD 	char

#define REX_OP	0x40
#define REX_B 	0x01
#define REX_X	0x02
#define REX_R 	0x04
#define REX_W	0x08

/* Note that while the register number we use in gtm can go from 0-15, the number of bits in the modrm/sib regfields are only 3.
 * So if we need to use the regs r8-r15, then a bit in the REX field should be set (depending on whether the register is encoded
 * in the sib or opcode or modrm, etc.
 */
#define SET_REX_PREFIX(REX_MANDATORY, REX_COND, reg)	\
{ \
  if (reg & 0x8) {  /* If bit 3 (index starting from 0) is set, set the specified REX bit */	\
	emit_base_info.rex |= REX_COND;		\
  } \
  emit_base_info.rex |= REX_OP | REX_MANDATORY; \
  emit_base_info.rex_set = 1; \
}

typedef union
{
	ModR_M		modrm;
	unsigned char	byte;
} modrm_byte_type;

typedef union
{
	SIB		sib;
	unsigned char	byte;
} sib_byte_type;

/* Instead of filling up the code buf, emit_base_offset will instead fill this structure, which will be used by the
 * macro which actually creates the instruction stream
 */
struct emit_base_info
{
	REX_FIELD rex;
	modrm_byte_type modrm_byte;
	sib_byte_type sib_byte;
	char offset8;
	int offset32;
	int64_t offset64;
	char imm8;
	int imm32;
	int64_t imm64;
	int rex_set;
	int modrm_byte_set;
	int sib_byte_set; /* Not using bitfields, since this would generate faster code. Since this is a one-type only
			   * created object, no real need for space optimization
			   */
	int offset8_set;
	int offset32_set;
	int offset64_set;
	int imm8_set;
	int imm32_set;
	int imm64_set;
};

GBLREF struct emit_base_info emit_base_info;

/* Define the macros for the instructions to be generated.. */

#define GENERIC_OPCODE_BEQ		1
#define GENERIC_OPCODE_BGE		2
#define GENERIC_OPCODE_BGT		3
#define GENERIC_OPCODE_BLE		4
#define GENERIC_OPCODE_BLT		5
#define GENERIC_OPCODE_BNE		6
#define GENERIC_OPCODE_BLBC		7
#define GENERIC_OPCODE_BLBS		8
#define GENERIC_OPCODE_BR		9
#define GENERIC_OPCODE_LDA		11
#define GENERIC_OPCODE_LOAD		12
#define GENERIC_OPCODE_STORE		13
#define GENERIC_OPCODE_STORE_ZERO	14
#define GENERIC_OPCODE_NOP		15

#define LONG_JUMP_OFFSET 		(0x7ffffffc)
#define MAX_OFFSET				0xffffffff
#define EMIT_JMP_ADJUST_BRANCH_OFFSET	branch_offset -= 5


#define EMIT_BASE_CODE_SIZE ((emit_base_info.rex_set ? 1 : 0) /* REX */ + 1 /* OPCODE */ + \
				(emit_base_info.modrm_byte_set ? 1 : 0)  /* MODRM */ + \
				(emit_base_info.sib_byte_set ? 1 : 0) + (emit_base_info.offset8_set ? 1 : 0) + \
				(emit_base_info.offset32_set ? 4 : 0) + (emit_base_info.offset64_set ? 8 : 0) + \
				(emit_base_info.imm8_set ? 1 : 0) + (emit_base_info.imm32_set ? 4 : 0) + \
				(emit_base_info.imm64_set ? 8 : 0))

#define CODE_BUF_GEN(op_code)			\
{ \
	(emit_base_info.rex_set) ? (code_buf[code_idx++] = emit_base_info.rex):0;	\
	code_buf[code_idx++] = (char) op_code;			\
	if (emit_base_info.modrm_byte_set)  { \
		code_buf[code_idx++] = emit_base_info.modrm_byte.byte; \
	} \
	if (emit_base_info.sib_byte_set) { \
		code_buf[code_idx++] = emit_base_info.sib_byte.byte; \
	} \
	if (emit_base_info.offset8_set) { \
		code_buf[code_idx++] = emit_base_info.offset8; \
	} \
	if (emit_base_info.offset32_set) { \
		*((int4 *)&code_buf[code_idx]) = emit_base_info.offset32; \
		code_idx += SIZEOF(int4) ;			\
	} \
	if (emit_base_info.offset64_set) { \
		*((int64_t *)&code_buf[code_idx]) = emit_base_info.offset64; \
		code_idx += SIZEOF(int64_t) ;			\
	} \
	if (emit_base_info.imm8_set) { \
		code_buf[code_idx++] = emit_base_info.imm8; \
	} \
	if (emit_base_info.imm32_set) { \
		*((int4 *)&code_buf[code_idx]) = emit_base_info.imm32; \
		code_idx += SIZEOF(int4) ;			\
	} \
	if (emit_base_info.imm64_set) { \
		*((int64_t *)&code_buf[code_idx]) = emit_base_info.imm64; \
		code_idx += SIZEOF(int64_t) ;			\
	} \
}

#define IGEN_LOAD_WORD_REG_8(reg) \
{ \
	SET_REX_PREFIX(REX_W, REX_R, reg)				\
	emit_base_info.modrm_byte.modrm.reg_opcode = reg & 0x7;	\
	CODE_BUF_GEN(I386_INS_MOV_Gv_Ev)				\
}
#define IGEN_LOAD_NATIVE_REG(reg)	IGEN_LOAD_WORD_REG_8(reg)

/* Sign extended load */
#define IGEN_LOAD_WORD_REG_4(reg) \
{ \
	SET_REX_PREFIX(REX_W, REX_R, reg)				\
	emit_base_info.modrm_byte.modrm.reg_opcode = reg & 0x7;	\
	CODE_BUF_GEN(I386_INS_MOVSXD_Gv_Ev)				\
}

#define IGEN_STORE_WORD_REG_8(reg) \
{ \
	SET_REX_PREFIX(REX_W, REX_R, reg)				\
	emit_base_info.modrm_byte.modrm.reg_opcode = reg & 0x7;	\
	CODE_BUF_GEN(I386_INS_MOV_Ev_Gv) \
}

#define IGEN_STORE_WORD_REG_4(reg) \
{ \
	SET_REX_PREFIX(0, REX_R, reg)					\
	emit_base_info.modrm_byte.modrm.reg_opcode = reg & 0x7;	\
	CODE_BUF_GEN(I386_INS_MOV_Ev_Gv) \
}

#define IGEN_STORE_ZERO_REG_8(reg) \
{ \
	SET_REX_PREFIX(REX_W, 0, 0)				\
	emit_base_info.imm32 = 0; \
	emit_base_info.imm32_set = 1; \
	CODE_BUF_GEN(I386_INS_MOV_Ev_Iv) \
}

#define IGEN_STORE_ZERO_REG_4(reg)	\
{ \
	emit_base_info.imm32 = 0;	\
	emit_base_info.imm32_set = 1;	\
	CODE_BUF_GEN(I386_INS_MOV_Ev_Iv)	\
}

#define IGEN_GENERIC_REG(opcode, reg) \
{\
	switch(opcode)  \
	{\
		case  GENERIC_OPCODE_LDA:\
			IGEN_LOAD_ADDR_REG(reg)\
			break;\
		case GENERIC_OPCODE_LOAD:\
			if ( next_ptr_offset == 4 ) {	\
				IGEN_LOAD_WORD_REG_4(reg);	\
			} else	{\
				IGEN_LOAD_WORD_REG_8(reg); \
			}	\
			next_ptr_offset = 8;	\
			break;\
		case GENERIC_OPCODE_STORE:\
			if ( next_ptr_offset == 4 ) {	\
				IGEN_STORE_WORD_REG_4(reg);	\
			} else	{\
				IGEN_STORE_WORD_REG_8(reg); \
			}	\
			next_ptr_offset = 8;	\
			break;\
		case GENERIC_OPCODE_STORE_ZERO:\
			if ( next_ptr_offset == 4 ) {	\
				IGEN_STORE_ZERO_REG_4(reg);	\
			} else	{\
				IGEN_STORE_ZERO_REG_8(reg); \
			}	\
			next_ptr_offset = 8;	\
			break;\
		default: /* which opcode ? */ \
			GTMASSERT;\
			break;\
	}\
}

#define IGEN_LOAD_LINKAGE(reg)	 	IGEN_LOAD_WORD_REG_8(reg)

#define IGEN_LOAD_ADDR_REG(reg)		\
{ \
	SET_REX_PREFIX(REX_W, REX_R, reg)					\
	emit_base_info.modrm_byte.modrm.reg_opcode = reg & 0x7;	\
	CODE_BUF_GEN(I386_INS_LEA_Gv_M) \
}

#define GEN_STORE_ARG(reg, offset)	\
{ \
	X86_64_ONLY(force_32 = TRUE;)	\
	GEN_STORE_WORD_8(reg, I386_REG_RSP, offset)	\
	X86_64_ONLY(force_32 = FALSE;)	\
}

#define GEN_LOAD_WORD_8(reg, base_reg, offset)	\
{ \
	emit_base_offset(base_reg, offset);				\
	SET_REX_PREFIX(REX_W, REX_R, reg)				\
	emit_base_info.modrm_byte.modrm.reg_opcode = reg & 0x7;	\
	CODE_BUF_GEN(I386_INS_MOV_Gv_Ev) \
}

/* Load 4 bytes with sign extension */
#define GEN_LOAD_WORD_4(reg, base_reg, offset)	\
{ \
	emit_base_offset(base_reg, offset);					\
	SET_REX_PREFIX(REX_W, REX_R, reg)				\
	emit_base_info.modrm_byte.modrm.reg_opcode = reg & 0x7;	\
	CODE_BUF_GEN(I386_INS_MOVSXD_Gv_Ev) \
}

#define GEN_STORE_WORD_8(reg, base_reg, offset)	\
{ \
	emit_base_offset(base_reg, offset);					\
	SET_REX_PREFIX(REX_W, REX_R, reg)				\
	emit_base_info.modrm_byte.modrm.reg_opcode = reg & 0x7;	\
	CODE_BUF_GEN(I386_INS_MOV_Ev_Gv) \
}

#define GEN_STORE_WORD_4(reg, base_reg, offset)	\
{ \
	emit_base_offset(base_reg, offset);					\
	SET_REX_PREFIX(0, REX_R, reg)					\
	emit_base_info.modrm_byte.modrm.reg_opcode = reg & 0x7;	\
	CODE_BUF_GEN(I386_INS_MOV_Ev_Gv) \
}

#define GEN_LOAD_IMMED(reg, imval)	\
{ \
	int op_code = I386_INS_MOV_eAX + (reg & 0x7); \
	memset((void *)&emit_base_info, 0, SIZEOF(emit_base_info));	\
	SET_REX_PREFIX(0, REX_B, reg) \
	emit_base_info.imm32 = (int) imval & 0xffffffff;	\
	emit_base_info.imm32_set = 1; \
	CODE_BUF_GEN(op_code)	\
}

#define GEN_CLEAR_WORD_EMIT(reg)	emit_trip(*(fst_opr + *inst++), TRUE, GENERIC_OPCODE_STORE_ZERO, reg)

#define GEN_LOAD_WORD_EMIT(reg)	 emit_trip(*(fst_opr + *inst++), TRUE, GENERIC_OPCODE_LOAD, reg)

#define EMIT_TRIP_ILIT_GEN		GEN_LOAD_IMMED(trg_reg, immediate)

#define GEN_XFER_TBL_CALL(xfer)	 \
{  \
	emit_base_offset(GTM_REG_XFER_TABLE, xfer); \
	emit_base_info.rex_set = 0;					\
	assert(GTM_REG_XFER_TABLE <= 7); /* if its a 64 bit register, we might need to set the REX bit */ \
	emit_base_info.modrm_byte.modrm.reg_opcode = (char) I386_INS_CALL_Ev; \
	CODE_BUF_GEN(I386_INS_Grp5_Prefix) \
}

#define GEN_CMP_EAX_IMM32(imm)	\
{ \
	int op_code = I386_INS_CMP_eAX_Iv ; \
	memset((void *)&emit_base_info, 0, SIZEOF(emit_base_info));	\
	emit_base_info.imm32 = (int4) imm & 0xffffffff;	\
	emit_base_info.imm32_set = 1; \
	CODE_BUF_GEN(op_code)	\
}



#define GEN_CMP_MEM64_ZERO(base_reg, offset)			\
{ \
	int op_code = I386_INS_Grp1_Ev_Iv_Prefix; \
	emit_base_offset(base_reg, offset); \
	SET_REX_PREFIX(REX_W, 0, 0) \
	emit_base_info.modrm_byte.modrm.reg_opcode = I386_INS_CMP__; \
	emit_base_info.imm32 = (int4) 0; \
	emit_base_info.imm32_set = 1; \
	CODE_BUF_GEN(op_code)	\
}

#define GEN_CMP_MEM32_ZERO(base_reg, offset)			\
{ \
	int op_code = I386_INS_Grp1_Ev_Iv_Prefix; \
	emit_base_offset(base_reg, offset); \
	emit_base_info.modrm_byte.modrm.reg_opcode = I386_INS_CMP__; \
	emit_base_info.imm32 = (int4) 0 ;  \
	emit_base_info.imm32_set = 1; \
	CODE_BUF_GEN(op_code)	\
}

#define GEN_CMP_REG_MEM32(reg, base_reg, offset)			\
{ \
	int op_code = I386_INS_CMP_Gv_Ev; \
	emit_base_offset(base_reg, offset); \
	SET_REX_PREFIX(0, REX_R, reg)					\
	emit_base_info.modrm_byte.modrm.reg_opcode = reg & 0x7;	\
	CODE_BUF_GEN(op_code)	\
}

#define GEN_CMP_REG_MEM64(reg, base_reg, offset)			\
{ \
	int op_code = I386_INS_CMP_Gv_Ev; \
	emit_base_offset(base_reg, offset); \
	SET_REX_PREFIX(REX_W, REX_R, reg)					\
	emit_base_info.modrm_byte.modrm.reg_opcode = reg & 0x7;	\
	CODE_BUF_GEN(op_code)	\
}

#define GEN_CMP_REGS(reg1, reg2) \
{ \
	int op_code = I386_INS_CMP_Gv_Ev; \
	memset((void *)&emit_base_info, 0, SIZEOF(emit_base_info));	\
	SET_REX_PREFIX(REX_W, REX_R, reg1)					\
	SET_REX_PREFIX(0, REX_B, reg2)					\
	emit_base_info.modrm_byte.modrm.reg_opcode = reg1 & 0x7;	\
	emit_base_info.modrm_byte.modrm.mod = I386_MOD32_REGISTER; \
	emit_base_info.modrm_byte.modrm.r_m = reg2 & 0x7;	\
	emit_base_info.modrm_byte_set = 1; \
	CODE_BUF_GEN(op_code)	\
}

/* Note that there is no CMP_IMM64 in AMD64 */
#define GEN_CMP_IMM32(reg, imm)			\
{ \
	int op_code = I386_INS_Grp1_Ev_Iv_Prefix; \
	memset((void *)&emit_base_info, 0, SIZEOF(emit_base_info));	\
	emit_base_info.modrm_byte.modrm.reg_opcode = I386_INS_CMP__; \
	emit_base_info.modrm_byte.modrm.mod = I386_MOD32_REGISTER; \
	emit_base_info.modrm_byte.modrm.r_m = reg & 0x7; \
	emit_base_info.modrm_byte_set = 1; \
	SET_REX_PREFIX(0, REX_B, reg) \
	emit_base_info.imm32 = (int4) imm & 0xffffffff;	\
	emit_base_info.imm32_set = 1; \
	CODE_BUF_GEN(op_code)	\
}


#define GEN_SUBTRACT_REGS(src1, src2, trgt)	GTMASSERT;

#define GEN_ADD_IMMED(reg, imval)		\
{ \
	int op_code = I386_INS_Grp1_Ev_Iv_Prefix; \
	memset((void *)&emit_base_info, 0, SIZEOF(emit_base_info)); \
	SET_REX_PREFIX(REX_W, REX_B, reg) \
	emit_base_info.modrm_byte_set = 1;	\
	emit_base_info.modrm_byte.modrm.mod = I386_MOD32_REGISTER;	\
	emit_base_info.modrm_byte.modrm.reg_opcode = I386_INS_ADD__;	\
	emit_base_info.modrm_byte.modrm.r_m = reg & 0x7; \
	emit_base_info.imm32 = (int4) imval;  \
	emit_base_info.imm32_set = 1; \
	CODE_BUF_GEN(op_code)	\
}

#define GEN_MOVE_REG(trg, src)	\
{ \
	int op_code = I386_INS_MOV_Ev_Gv; \
	memset((void *)&emit_base_info, 0, SIZEOF(emit_base_info)); \
	SET_REX_PREFIX(REX_W, REX_B, trg) \
	SET_REX_PREFIX(0, REX_R, src) \
	emit_base_info.modrm_byte_set = 1;	\
	emit_base_info.modrm_byte.modrm.mod = I386_MOD32_REGISTER;	\
	emit_base_info.modrm_byte.modrm.reg_opcode = src;	\
	emit_base_info.modrm_byte.modrm.r_m = trg & 0x7;	\
	CODE_BUF_GEN(op_code)	\
}

#define GEN_JUMP_REG(reg)	\
{ \
	int op_code = I386_INS_Grp5_Prefix; \
	memset((void *)&emit_base_info, 0, SIZEOF(emit_base_info)); \
	SET_REX_PREFIX(REX_OP | REX_W, REX_B, reg) \
	emit_base_info.modrm_byte_set = 1;	\
	emit_base_info.modrm_byte.modrm.mod = I386_MOD32_REGISTER;	\
	emit_base_info.modrm_byte.modrm.reg_opcode = I386_INS_JMP_Ev;	\
	emit_base_info.modrm_byte.modrm.r_m = reg & 0x7;	\
	CODE_BUF_GEN(op_code)	\
}

#define GEN_PCREL		\
{ \
	int op_code = I386_INS_CALL_Jv; \
	memset((void *)&emit_base_info, 0, SIZEOF(emit_base_info)); \
	emit_base_info.imm32 = 0; \
	emit_base_info.imm32_set = 1; \
	CODE_BUF_GEN(op_code)	\
	memset((void *)&emit_base_info, 0, SIZEOF(emit_base_info)); \
	op_code = I386_INS_POP_eAX + (GTM_REG_CODEGEN_TEMP & 0x7);	\
	SET_REX_PREFIX(0, REX_B, GTM_REG_CODEGEN_TEMP) \
	assert(GTM_REG_CODEGEN_TEMP > 7); \
	CODE_BUF_GEN(op_code) \
}

/*
 * GT.M on AIX and SPARC is 64bit
 * By default the loads/stores use ldd/std(load double),
 * but if the value being dealt with is a word,the
 * opcode in generic_inst is changed to ldw/stw
 * On other platforms, it is defined to null
 */
#define REVERT_GENERICINST_TO_WORD(inst)
