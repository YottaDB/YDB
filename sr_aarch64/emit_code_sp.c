/****************************************************************
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

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stdlib.h"

#include "cgp.h"
#include "compiler.h"
#include <rtnhdr.h>
#include "vxt.h"
#include "list_file.h"
#include <emit_code.h>

GBLREF uint4		code_buf[];	/* Instruction buffer */
GBLREF int		code_idx;
GBLREF int		jmp_offset;	/* Offset to jump target */
GBLREF int		code_idx;	/* Index into code_buf */
GBLREF char		cg_phase;	/* Current compiler phase */
GBLREF int4		curr_addr;
#ifdef DEBUG
GBLREF unsigned char    *obpt;		/* output buffer index */
GBLREF unsigned char    outbuf[];	/* assembly language output buffer */
static unsigned int	ains;		/* assembler instruction (binary) */
#endif

void	emit_base_offset_addr(int base, int offset)
{
	int		abs_offst, immed12;

	switch (cg_phase)
	{
#ifdef DEBUG
		case CGP_ASSEMBLY:
#endif
		case CGP_ADDR_OPT:
		case CGP_APPROX_ADDR:
		case CGP_MACHINE:
			assert(base >= 0  &&  base <= 31);		/* register is in correct range */
			abs_offst = abs(offset);
			if (0 <= offset)
			{
				code_buf[code_idx++] = CODE_BUF_GEN_D_IMM16(AARCH64_INS_MOV_IMM, GTM_REG_CODEGEN_TEMP_1,
									    offset & 0xffff);
			} else
			{
				code_buf[code_idx++] = CODE_BUF_GEN_D_IMM16(AARCH64_INS_MOV_INV, GTM_REG_CODEGEN_TEMP_1,
									    (abs_offst - 1) & 0xffff);
			}
			if (MAX_16BIT < abs_offst)
			{
				code_buf[code_idx++] = CODE_BUF_GEN_D_IMM16_SHIFT(AARCH64_INS_MOVK, GTM_REG_CODEGEN_TEMP_1,
								(offset & 0xffff0000) >> 16, 0x1);
				if (MAX_32BIT < abs_offst)
				{
					code_buf[code_idx++] = CODE_BUF_GEN_D_IMM16_SHIFT(AARCH64_INS_MOVK, GTM_REG_CODEGEN_TEMP_1,
									(offset & 0xffff00000000) >> 32, 0x2);
					if (MAX_48BIT < abs_offst)
					{
						code_buf[code_idx++] = CODE_BUF_GEN_D_IMM16_SHIFT(AARCH64_INS_MOVK,
										GTM_REG_CODEGEN_TEMP_1,
										(offset & 0xffff000000000000) >> 48, 0x3);
					}
				}
			}
			code_buf[code_idx] = CODE_BUF_GEN_DNM(0, 0, base, GTM_REG_CODEGEN_TEMP_1);
			break;
		default:
			GTMASSERT;
	}
}

void	emit_base_offset_load(int base, int offset)
{
	int		abs_offst, immed12;

	switch (cg_phase)
	{
#ifdef DEBUG
		case CGP_ASSEMBLY:
#endif
		case CGP_ADDR_OPT:
		case CGP_APPROX_ADDR:
		case CGP_MACHINE:
			assert(base >= 0  &&  base <= 31);		/* register is in correct range */
			abs_offst = abs(offset);
#if 0
			if (MAX_12BIT >= abs_offst)
			{
				if (0 != offset)
				{
					code_buf[code_idx++] = CODE_BUF_GEN_DN_IMM12(((0 < offset)
										      ? AARCH64_INS_ADD_IMM : AARCH64_INS_SUB_IMM),
										     GTM_REG_CODEGEN_TEMP_1, base, offset);
					code_buf[code_idx] = CODE_BUF_GEN_TN_IMM12(0, 0, GTM_REG_CODEGEN_TEMP_1, 0);
				} else
				{
					code_buf[code_idx] = CODE_BUF_GEN_TN_IMM12(0, 0, base, 0);
				}
#endif
			if (MAX_12BIT >= abs_offst)
			{
				/* Assume this will be an 4 byte operation. If it turns out to be 8, the IGEN_GENERIC_REG macro
				   will adjust offset	xxxxxxx trying an experiment with no shift xxxxxxx
				*/
				code_buf[code_idx] = CODE_BUF_GEN_TN0_IMM12(0, 0, base, offset);
			} else
			{
				code_buf[code_idx++] = CODE_BUF_GEN_D_IMM16(AARCH64_INS_MOV_IMM, GTM_REG_CODEGEN_TEMP_1, offset);
				if (MAX_16BIT < abs_offst)
				{
					code_buf[code_idx++] = CODE_BUF_GEN_D_IMM16_SHIFT(AARCH64_INS_MOVK, GTM_REG_CODEGEN_TEMP_1,
								(offset & 0xffff0000) >> 16, 0x1);
					if (MAX_32BIT < abs_offst)
					{
						code_buf[code_idx++] = CODE_BUF_GEN_D_IMM16_SHIFT(AARCH64_INS_MOVK,
										GTM_REG_CODEGEN_TEMP_1,
										(offset & 0xffff00000000) >> 32, 0x2);
						if (MAX_48BIT < abs_offst)
						{
							code_buf[code_idx++] = CODE_BUF_GEN_D_IMM16_SHIFT(AARCH64_INS_MOVK,
											GTM_REG_CODEGEN_TEMP_1,
											(offset & 0xffff000000000000) >> 48, 0x3);
						}
					}
				}
				code_buf[code_idx++] = (((0 <= offset) ? AARCH64_INS_ADD_REG : AARCH64_INS_SUB_REG)
								| (GTM_REG_CODEGEN_TEMP_1 << AARCH64_SHIFT_RM)
								| (GTM_REG_CODEGEN_TEMP_1 << AARCH64_SHIFT_RD)
								| (base << AARCH64_SHIFT_RN));
				code_buf[code_idx] = CODE_BUF_GEN_N(0, GTM_REG_CODEGEN_TEMP_1);
 			}
			break;
		default:
			GTMASSERT;
	}
}


#ifdef DEBUG
void fmt_reg(int reg, int size, int z_flag)
{
	switch(reg)
	{
		/* case 29:	xxxxxxx */			/* fp */
			/* *obpt++ = 'f';
			*obpt++ = 'p';
			break; xxxxxxx */
		case 30:				/* lr */
			*obpt++ = 'l';
			*obpt++ = 'r';
			break;
		case 31:				/* sp  or [wx]zr */
			if (0 == z_flag)
			{
		  		*obpt++ = (0 == size) ? 'w' : 'x';
		  		*obpt++ = 'z';
		  		*obpt++ = 'r';
			} else
			{
				*obpt++ = 's';
				*obpt++ = 'p';
			}
			break;
		default:
			*obpt++ = size ? 'x' : 'w';
			obpt = i2asc(obpt, reg);
	}
}

void fmt_rd(int size)
{
	int reg;

	reg = GET_RD(ains);
	fmt_reg(reg, size, 1);
}

void fmt_rt(int size, int z_flag)
{
	int reg;

	reg = GET_RT(ains);
	fmt_reg(reg, size, z_flag);
}

void fmt_rt2(int size)
{
	int reg;

	reg = GET_RT2(ains);
	fmt_reg(reg, size, 1);
}

void fmt_rn(int size)
{
	int reg;

	reg = GET_RN(ains);
	fmt_reg(reg, size, 1);
}

void fmt_rm(int size)
{
	int reg;

	reg = GET_RM(ains);
	fmt_reg(reg, size, 1);
}

void fmt_rd_rn(int size)
{
	fmt_rd(size);
	*obpt++ = ',';
	obpt++;
	fmt_rn(size);
}

void fmt_rd_rm(int size)
{
	fmt_rd(size);
	*obpt++ = ',';
	obpt++;
	fmt_rm(size);
}

void fmt_rn_rm(int size)
{
	fmt_rn(size);
	*obpt++ = ',';
	obpt++;
	fmt_rm(size);
}

void fmt_rd_rn_rm(int size)
{
	fmt_rd_rn(size);
	*obpt++ = ',';
	obpt++;
	fmt_rm(size);
}

void fmt_rd_rn_rm_sxtw()
{
	fmt_rd_rn(1);
	*obpt++ = ',';
	obpt++;
	fmt_rm(0);
	*obpt++ = ',';
	obpt++;
	*obpt++ = 's';
	*obpt++ = 'x';
	*obpt++ = 't';
	*obpt++ = 'w';
}

void fmt_rn_raw_imm12(int size)
{
	fmt_rn(size);
	*obpt++ = ',';
	obpt++;
	*obpt++ = '#';
	obpt = i2asc(obpt, GET_IMM12(ains) * 8);
}

void fmt_rn_shift_imm12(int size)
{
	fmt_rn(size);
	*obpt++ = ',';
	obpt++;
	*obpt++ = '#';
	obpt = i2asc(obpt, GET_IMM12(ains) * 8);
}

void fmt_rd_rn_shift_immr(int size)
{
	int immed;

	fmt_rd_rn(size);
	*obpt++ = ',';
	obpt++;
	*obpt++ = '#';
	immed = GET_IMMR(ains);
	obpt = i2asc(obpt, immed);
}

/* xxxxxxx
void fmt_rd_rn_shift_imm12(int size)
{
	fmt_rd_rn(size);
	*obpt++ = ',';
	obpt++;
	*obpt++ = '#';
	obpt = i2asc(obpt, GET_IMM12(ains) * 8);
}
xxxxxxx */

void fmt_rd_rn_imm12(int size)
{
	fmt_rd_rn(size);
	*obpt++ = ',';
	obpt++;
	*obpt++ = '#';
	obpt = i2asc(obpt, GET_IMM12(ains));
}

void fmt_rt_rt2_rn_shift_imm7(int size)
{
	int immed;

	fmt_rt(size, 1);
	*obpt++ = ',';
	obpt++;
	fmt_rt2(size);
	*obpt++ = ',';
	obpt++;
	*obpt++ = '[';
	fmt_rn(size);
	*obpt++ = ']';
	immed = GET_IMM7(ains) * 8;
	if (0 != immed)
	{
		*obpt++ = ',';
		*obpt++ = ' ';
		*obpt++ = '#';
		obpt = i2asc(obpt, immed);
	}
}

void fmt_rt_rn_raw_imm12(int size, int mult)
{
	int immed;

	fmt_rt(size, 0);
	*obpt++ = ',';
	obpt++;
	*obpt++ = '[';
	fmt_rn(1);
	immed = GET_IMM12(ains) * mult;
	if (0 != immed)
	{
		*obpt++ = ',';
		*obpt++ = ' ';
		*obpt++ = '#';
		obpt = i2asc(obpt, immed);
	}
	*obpt++ = ']';
}

void fmt_rd_shift_imm12(int size)
{
	fmt_rd(size);
	*obpt++ = ',';
	obpt++;
	*obpt++ = '#';
	obpt = i2asc(obpt, GET_IMM12(ains) * 8);
}

void fmt_rd_raw_imm16(int size)
{
	int	imm16;

	fmt_rd(size);
	*obpt++ = ',';
	obpt++;
	imm16 = GET_IMM16(ains);
	*obpt++ = '#';
	obpt = i2asc(obpt, imm16);
	tab_to_column(55);
	*obpt++ = '0';
	*obpt++ = 'x';
	obpt += i2hexl_nofill(imm16, obpt, (0 == size) ? 8 : 16);
}

void fmt_rd_raw_imm16_inv(int size)
{
	int	imm16;

	fmt_rd(size);
	*obpt++ = ',';
	obpt++;
	imm16 = GET_IMM16(ains) + 1;
	*obpt++ = '#';
	*obpt++ = '-';
	obpt = i2asc(obpt, imm16);
	tab_to_column(55);
	*obpt++ = '0';
	*obpt++ = 'x';
	obpt += i2hexl_nofill(-1 * imm16, obpt, (0 == size) ? 8 : 16);
}

void fmt_brdisp()
{
	obpt++;
	*obpt++ = '0';
	*obpt++ = 'x';
	obpt += i2hexl_nofill((((GET_BRDISP(ains) - 1) * 4 + curr_addr - SIZEOF(rhdtyp)) & AARCH64_MASK_BRANCH_DISP), obpt, 16);
}

void fmt_brdispcond()
{
	obpt++;
	*obpt++ = '0';
	*obpt++ = 'x';
	obpt += i2hexl_nofill(((GET_BRDISPCOND(ains) - 1) * 4 + curr_addr - SIZEOF(rhdtyp)), obpt, 16);
}

void fmt_ains()
{
	obpt++;
	*obpt++ = '0';
	*obpt++ = 'x';
	obpt += i2hex_nofill(ains, obpt, 8);
}

void tab_to_column(int col)
{
	int		offset;

	offset = (int)(&outbuf[0] + col - obpt);
	if (offset >= 1)
	{	/* tab to position "col" */
		memset(obpt, ' ', offset);
		obpt += offset;
	} else
	{	/* leave at least 2 spaces */
		memset(obpt, ' ', 2);
		obpt += 2;
	}
	*obpt++ = ';';
}

void 	format_machine_inst(void)
{
  int	instindx, shift, size, opcode;

	for (instindx = 0; instindx < code_idx; instindx++)
	{
		list_chkpage();
		obpt = &outbuf[0];
		memset(obpt, SP, ASM_OUT_BUFF);
		obpt += 10;
		i2hex((curr_addr - SIZEOF(rhdtyp)), (uchar_ptr_t)obpt, 8);
		curr_addr += 4;
		obpt += 10;
		i2hex(code_buf[instindx], (uchar_ptr_t)obpt, 8);
		obpt += 10;
		ains = code_buf[instindx];
		size = 0;			/* Assume 32 bit registers until the opcode says otherwise */
		switch(opcode = GET_OPCODE(ains))
		{
			case 0x91:	/* ADD immediate */
				if (0 == GET_IMM12(ains))
				{
					memcpy(obpt, MOV_INST, SIZEOF(MOV_INST) - 1);
					obpt += OPSPC;
					fmt_rd_rn(1);
				} else
				{
					memcpy(obpt, ADD_INST, SIZEOF(ADD_INST) - 1);
					obpt += OPSPC;
					fmt_rd_rn_imm12(1);
				}
				break;
			case 0x8b:	/* ADD register */
				memcpy(obpt, ADD_INST, SIZEOF(ADD_INST) - 1);
				obpt += OPSPC;
				if (0x2 == ((ains >> AARCH64_SHIFT_OP_20) & 0xe))
				{
					fmt_rd_rn_rm_sxtw();
				} else
				{
					fmt_rd_rn_rm(1);
				}
				break;
			case 0x10:	/* ADR */
				memcpy(obpt, ADR_INST, SIZEOF(SUB_INST) - 1);
				obpt += OPSPC;
				fmt_rd_raw_imm16(1);
				break;
			case 0x14:	/* B */
				memcpy(obpt, B_INST, SIZEOF(B_INST) - 1);
				obpt += OPSPC;
				fmt_brdisp();
				break;
			case 0x54:	/* BEQ, BNE, BGE, BGT, BLE, BLT */
				switch(GET_COND(ains))
				{
					case AARCH64_COND_EQ:
						memcpy(obpt, BEQ_COND, SIZEOF(BEQ_COND) - 1);
						break;
					case AARCH64_COND_NE:
						memcpy(obpt, BNE_COND, SIZEOF(BNE_COND) - 1);
						break;
					case AARCH64_COND_GT:
						memcpy(obpt, BGT_COND, SIZEOF(BGT_COND) - 1);
						break;
					case AARCH64_COND_GE:
						memcpy(obpt, BGE_COND, SIZEOF(BGE_COND) - 1);
						break;
					case AARCH64_COND_LT:
						memcpy(obpt, BLT_COND, SIZEOF(BLT_COND) - 1);
						break;
					case AARCH64_COND_LE:
						memcpy(obpt, BLE_COND, SIZEOF(BLE_COND) - 1);
						break;
					default:
						GTMASSERT;
				}
				obpt += OPSPC;
				fmt_brdispcond();
				break;
			case 0x94:	/* BL */
  				memcpy(obpt, BL_INST, SIZEOF(BL_INST) - 1);
				obpt += OPSPC;
				fmt_brdisp();
				break;
			case 0xd6:	/* BLR, BR */
				switch((ains >> AARCH64_SHIFT_OP_20) & 0xf)
				{
					case 0x1:	/* BR */
  						memcpy(obpt, BR_INST, SIZEOF(BR_INST) - 1);
						break;
					case 0x3:	/* BLR */
  						memcpy(obpt, BLR_INST, SIZEOF(BLR_INST) - 1);
						break;
					default:
						GTMASSERT;
				}
				obpt += OPSPC;
				fmt_rn(1);
				break;
			case 0x31:	/* CMN immediate */
				memcpy(obpt, CMN_INST, SIZEOF(CMN_INST) - 1);
				obpt += OPSPC;
				fmt_rn_shift_imm12(0);
				break;
			case 0xab:	/* CMN register */
				memcpy(obpt, CMN_INST, SIZEOF(CMN_INST) - 1);
				obpt += OPSPC;
				fmt_rn_rm(1);
				break;
			case 0x71:	/* CMP immediate */
				memcpy(obpt, CMP_INST, SIZEOF(CMP_INST) - 1);
				obpt += OPSPC;
				fmt_rn_shift_imm12(0);
				break;
			case 0xeb:	/* CMP register */
				memcpy(obpt, CMP_INST, SIZEOF(CMP_INST) - 1);
				obpt += OPSPC;
				fmt_rn_rm(1);
				break;
			case 0xa8:	/* LDP STP */
				switch((ains >> AARCH64_SHIFT_OP_20) & 0xf)
				{
					case 0x8:	/* STP */
  						memcpy(obpt, STP_INST, SIZEOF(STP_INST) - 1);
						break;
					case 0xc:	/* LDP */
  						memcpy(obpt, LDP_INST, SIZEOF(LDP_INST) - 1);
						break;
					default:
						GTMASSERT;
				}
				obpt += OPSPC;
				fmt_rt_rt2_rn_shift_imm7(1);
				break;
			case 0xf9:	/* LDR STR 64 bits */
				size = 1;
			case 0xb9:	/* LDR STR 32 bits */
				switch((ains >> AARCH64_SHIFT_OP_20) & 0xc)
				{
					case 0x0:	/* STR */
  						memcpy(obpt, STR_INST, SIZEOF(STR_INST) - 1);
						obpt += OPSPC;
						fmt_rt_rn_raw_imm12(size, (1 == size) ? 8 : 4);
						break;
					case 0x4:	/* LDR */
  						memcpy(obpt, LDR_INST, SIZEOF(LDR_INST) - 1);
						obpt += OPSPC;
						fmt_rt_rn_raw_imm12(size, (1 == size) ? 8 : 4);
						break;
					case 0x8:	/* LDRSW */
						size = 1;
  						memcpy(obpt, LDRSW_INST, SIZEOF(LDRSW_INST) - 1);
						obpt += OPSPC;
						fmt_rt_rn_raw_imm12(size, 4);
						break;
					default:
						GTMASSERT;
				}
				break;

			case 0xd3:	/* LSR - 64 bit */
				size = 1;
			case 0x53:	/* LSR - 32 bit */
				memcpy(obpt, LSR_INST, SIZEOF(LSR_INST) - 1);
				obpt += OPSPC;
				fmt_rd_rn_shift_immr(size);
				break;
				

			case 0x52:	/* MOV imm */
				memcpy(obpt, MOV_INST, SIZEOF(MOV_INST) - 1);
				obpt += OPSPC;
				fmt_rd_raw_imm16(0);
				break;

			case 0x12:	/* MOV inverted */
				memcpy(obpt, MOV_INST, SIZEOF(MOV_INST) - 1);
				obpt += OPSPC;
				fmt_rd_raw_imm16_inv(0);
				break;

			case 0xaa:	/* MOV register */
				memcpy(obpt, MOV_INST, SIZEOF(MOV_INST) - 1);
				obpt += OPSPC;
				fmt_rd_rm(1);
				break;

			case 0x72:	/* MOVK */
				memcpy(obpt, MOVK_INST, SIZEOF(MOVK_INST) - 1);
				obpt += OPSPC;
				fmt_rd_raw_imm16(0);
				break;

			case 0xd5:	/* NOP */
				memcpy(obpt, NOP_INST, SIZEOF(NOP_INST) - 1);
				break;

			case 0xd1:	/* SUB immediate */
				memcpy(obpt, SUB_INST, SIZEOF(SUB_INST) - 1);
				obpt += OPSPC;
				fmt_rd_rn_imm12(1);
				break;

			case 0xcb:	/* SUB register */
				memcpy(obpt, SUB_INST, SIZEOF(SUB_INST) - 1);
				obpt += OPSPC;
				fmt_rd_rn_rm(1);
				if (0x2 == (ains >> AARCH64_SHIFT_OP_20) & 0xf)
				{
					*obpt++ = ',';
					obpt++;
					*obpt++ = 'L';
					*obpt++ = 'S';
					*obpt++ = 'L';
					obpt++;
					*obpt++ = '#';
					obpt = i2asc(obpt, shift);
				}
				break;

			default:
				if (0x14 == (opcode & 0xfc))
				{	/* Low bits of branch opcode were part of displacement */
					memcpy(obpt, B_INST, SIZEOF(B_INST) - 1);
					obpt += OPSPC;
					fmt_brdisp();
				} else	/* Unknown (to us) op code */
				{
					memcpy(obpt, INV_INST, SIZEOF(INV_INST) - 1);
					obpt += SIZEOF(INV_INST);
					fmt_ains();
				}
				break;
		}
		emit_eoi();
	}
}
#endif

