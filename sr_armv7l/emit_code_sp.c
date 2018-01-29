/****************************************************************
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

int	encode_immed12(int offset)
{
	uint4 encode;
	int rotate;

	encode = offset;
	for (rotate = 0; 32 > rotate; rotate += 2)
	{
		if (!(encode & ~0xff))
		{
			return (rotate / 2) * 256 + encode;
		}
		/* rotate left by two */
		encode = (encode << 2) | (encode >> 30);
	}

	/* offset isn't expressible as ARM imm12 */
	return -1;
}

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
			assert(base >= 0  &&  base <= 15);		/* register is in correct range */
			abs_offst = abs(offset);
			if (256 > abs_offst)
			{
				code_buf[code_idx++] = ARM_INS_MOV_IMM
					| (abs_offst << ARM_SHIFT_IMM12)
					| (GTM_REG_CODEGEN_TEMP_1 << ARM_SHIFT_RD);
			} else
#			ifdef __armv7l__
			{
				/* Always use movw - add sequence because shrink trips can convert a movw add sequence to
				 * add r1, r1, #xxx if the adjusted offset now falls within the weird ARM shifted 12 bit range.
				 * But now the next shrink trips pass can change it back to movw - add. Now the old code size
				 * will be smaller than the new, leading to an assert in shrink_trips().
				 */
				code_buf[code_idx++] = ARM_INS_MOVW
						| (((abs_offst >> 12) & ARM_MASK_IMM4) << ARM_SHIFT_IMM4)
						| (GTM_REG_CODEGEN_TEMP_1 << ARM_SHIFT_RD)
						| ((abs_offst & ARM_MASK_IMM12) << ARM_SHIFT_IMM12);
				if (65535 < abs_offst)
				{
					code_buf[code_idx++] = ARM_INS_MOVT
						| (((abs_offst >> 28) & ARM_MASK_IMM4) << ARM_SHIFT_IMM4)
						| (GTM_REG_CODEGEN_TEMP_1 << ARM_SHIFT_RD)
						| (((abs_offst >> 16) & ARM_MASK_IMM12) << ARM_SHIFT_IMM8);
				}
			}
#			else    /* __armv6l__ */
			{
				code_buf[code_idx++] = ARM_INS_LDR
					| (GTM_REG_CODEGEN_TEMP_1 << ARM_SHIFT_RT)
					| (ARM_REG_PC << ARM_SHIFT_RN);
				code_buf[code_idx++] = ARM_INS_B;
				code_buf[code_idx++] = abs(offset);
			}
#			endif
			code_buf[code_idx] = (((0 <= offset) ? ARM_U_BIT_ON : ARM_B_BIT_ON)
							| (GTM_REG_CODEGEN_TEMP_1 << ARM_SHIFT_RM)
							| (base << ARM_SHIFT_RN));
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
			assert(base >= 0  &&  base <= 15);		/* register is in correct range */
			abs_offst = abs(offset);
			if (4096 > abs_offst)
			{
				code_buf[code_idx] = (((offset >= 0) ? ARM_U_BIT_ON : 0)
							| ((abs_offst & ARM_MASK_IMM12) << ARM_SHIFT_IMM12)
							| (base << ARM_SHIFT_RN));
			} else
			{
#ifdef __armv7l__
				code_buf[code_idx++] = ARM_INS_MOVW
								| (((abs_offst >> 12) & ARM_MASK_IMM4) << ARM_SHIFT_IMM4)
								| (GTM_REG_CODEGEN_TEMP_1 << ARM_SHIFT_RD)
								| ((abs_offst & ARM_MASK_IMM12) << ARM_SHIFT_IMM12);
				if (65535 < abs_offst)
				{
					code_buf[code_idx++] = ARM_INS_MOVT
									| (((abs_offst >> 28) & ARM_MASK_IMM4) << ARM_SHIFT_IMM4)
									| (GTM_REG_CODEGEN_TEMP_1 << ARM_SHIFT_RD)
									| (((abs_offst >> 16) & ARM_MASK_IMM12) << ARM_SHIFT_IMM8);
				}
#else	/* __armv6l__ */
				code_buf[code_idx++] = ARM_INS_LDR
							| (GTM_REG_CODEGEN_TEMP_1 << ARM_SHIFT_RT)
							| (ARM_REG_PC << ARM_SHIFT_RN);
				code_buf[code_idx++] = ARM_INS_B;
				code_buf[code_idx++] = abs_offst;
#endif
				code_buf[code_idx++] = (((0 <= offset) ? ARM_INS_ADD_REG : ARM_INS_SUB_REG)
								| (GTM_REG_CODEGEN_TEMP_1 << ARM_SHIFT_RM)
								| (GTM_REG_CODEGEN_TEMP_1 << ARM_SHIFT_RD)
								| (base << ARM_SHIFT_RN));
				code_buf[code_idx] = (GTM_REG_CODEGEN_TEMP_1 << ARM_SHIFT_RN);
 			}
			break;
		default:
			GTMASSERT;
	}
}


#ifdef DEBUG
void fmt_reg(int reg)
{
	switch(reg)
	{
		case 11:				/* fp */
			*obpt++ = 'f';
			*obpt++ = 'p';
			break;
		case 13:				/* sp */
			*obpt++ = 's';
			*obpt++ = 'p';
			break;
		case 14:				/* lr */
			*obpt++ = 'l';
			*obpt++ = 'r';
			break;
		case 15:				/* pc */
			*obpt++ = 'p';
			*obpt++ = 'c';
			break;
		default:
			*obpt++ = 'r';
			obpt = i2asc(obpt, reg);
	}
}

void fmt_rd()
{
	int reg;

	reg = GET_RD(ains);
	fmt_reg(reg);
}

void fmt_rt()
{
	int reg;

	reg = GET_RT(ains);
	fmt_reg(reg);
}

void fmt_rn()
{
	int reg;

	reg = GET_RN(ains);
	fmt_reg(reg);
}

void fmt_rm()
{
	int reg;

	reg = GET_RM(ains);
	fmt_reg(reg);
}

void fmt_rd_rn()
{
	fmt_rd();
	*obpt++ = ',';
	obpt++;
	fmt_rn();
}

void fmt_rd_rm()
{
	fmt_rd();
	*obpt++ = ',';
	obpt++;
	fmt_rm();
}

void fmt_rn_rm()
{
	fmt_rn();
	*obpt++ = ',';
	obpt++;
	fmt_rm();
}

void fmt_rd_rn_rm()
{
	fmt_rd_rn();
	*obpt++ = ',';
	obpt++;
	fmt_rm();
}

int decode_immed12(int offset)
{
	int immed12, rotate;

	immed12 = (offset & 0xff);
	rotate = (offset >> 8) * 2;
	if (rotate > 0)
	{
		for (;  32 > rotate; rotate += 2)
		{
			immed12 = (immed12 >> 30) | (immed12 << 2);
		}
	}

	return immed12;
}

void fmt_sregs()
{
	int	immed8;

	immed8 = GET_IMM8(ains);
	*obpt++ = 's';
	*obpt++ = '0';
	*obpt++ = '-';
	*obpt++ = 's';
	obpt = i2asc(obpt, immed8 - 1);
}

void fmt_rn_raw_imm12()
{
	int immed;

	fmt_rn();
	*obpt++ = ',';
	obpt++;
	*obpt++ = '#';
	immed = GET_IMM12(ains);
	obpt = i2asc(obpt, immed);
}

void fmt_rn_shift_imm12()
{
	fmt_rn();
	*obpt++ = ',';
	obpt++;
	*obpt++ = '#';
	obpt = i2asc(obpt, decode_immed12(GET_IMM12(ains)));
}

void fmt_rd_rm_shift_imm5()
{
	int immed;

	fmt_rd_rm();
	*obpt++ = ',';
	obpt++;
	*obpt++ = '#';
	immed = GET_IMM5(ains);
	obpt = i2asc(obpt, immed);
}

void fmt_rd_rn_shift_imm12()
{
	fmt_rd_rn();
	*obpt++ = ',';
	obpt++;
	*obpt++ = '#';
	obpt = i2asc(obpt, decode_immed12(GET_IMM12(ains)));
}

void fmt_rt_rn_raw_imm12()
{
	int immed;

	fmt_rt();
	*obpt++ = ',';
	obpt++;
	*obpt++ = '[';
       fmt_rn();
	immed = GET_IMM12(ains);
	if (0 != immed)
	{
		*obpt++ = ',';
		*obpt++ = ' ';
		*obpt++ = '#';
		if (0 == (ains & ARM_U_BIT_ON))
		{
			*obpt++ = '-';
			immed = -1 * immed;
		}
		obpt = i2asc(obpt, immed);
	}
	*obpt++ = ']';
	if ((0 != (ains & ARM_P_BIT_ON)) && (0 != (ains & ARM_W_BIT_ON)))
	{
		*obpt++ = '!';
	}
}

void fmt_rd_shift_imm12()
{
	fmt_rd();
	*obpt++ = ',';
	obpt++;
	*obpt++ = '#';
	obpt = i2asc(obpt, decode_immed12(GET_IMM12(ains)));
}

void fmt_rd_raw_imm16()
{
	int		imm16;

	fmt_rd();
	*obpt++ = ',';
	obpt++;
	imm16 = GET_IMM16(ains);
	obpt = i2asc(obpt, imm16);
	tab_to_column(55);
	*obpt++ = '0';
	*obpt++ = 'x';
	obpt += i2hex_nofill(imm16, obpt, 6);
}

void fmt_const()
{
	obpt = i2asc(obpt, ains);
	tab_to_column(55);
	fmt_ains();
}

void fmt_brdisp()
{
	*obpt++ = '0';
	*obpt++ = 'x';
	obpt += i2hex_nofill(((GET_BRDISP(ains) + 1 ) * 4 + curr_addr - SIZEOF(rhdtyp)), obpt, 6);
}

void fmt_ains()
{
	obpt++;
	*obpt++ = '0';
	*obpt++ = 'x';
	obpt += i2hex_nofill(ains, obpt, 8);
}

void fmt_registers()
{
	int		reg, registers, reg_bit;
	boolean_t	found_reg = FALSE;

	registers = GET_REGISTERS(ains);
	reg_bit = 1;
	for (reg = 0; 15 >= reg; reg++, reg_bit = reg_bit << 1)
	{
		if (registers & reg_bit)
		{
			if (found_reg)
			{
				*obpt++ = ',';
				*obpt++ = ' ';
			} else
			{
				found_reg = TRUE;
			}
			fmt_reg(reg);
		}
	}
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
	int	instindx, shift, load_const;

	load_const = 0;
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
		if (2 == load_const)
		{
			load_const = 0;
			fmt_const();
			emit_eoi();
			continue;
		}
		switch(GET_OPCODE(ains))
		{
			case 0x04:	/* SUB register */
				memcpy(obpt, SUB_INST, SIZEOF(SUB_INST) - 1);
				shift =  (0 != (0xf800 & ains)) ? GET_IMM5(ains) : 0;
				obpt += OPSPC;
				fmt_rd_rn_rm();
				if (0 < shift)
				{
					*obpt++ = ',';
					obpt++;
					memcpy(obpt, LSL_SHIFT_TYPE, SIZEOF(LSL_SHIFT_TYPE) - 1);
					obpt += SIZEOF(LSL_SHIFT_TYPE);
					*obpt++ = '#';
					obpt = i2asc(obpt, shift);
				}
				break;
			case 0x05:	/* SUBS register */
				memcpy(obpt, SUBS_INST, SIZEOF(SUBS_INST) - 1);
				obpt += OPSPC;
				fmt_rd_rn_rm();
				break;
			case 0x08:	/* ADD register */
				memcpy(obpt, ADD_INST, SIZEOF(ADD_INST) - 1);
				obpt += OPSPC;
				fmt_rd_rn_rm();
				break;
			case 0x12:	/* BLX & BX */
				if (0xfff30 == (0xffff0 & ains))
				{
					memcpy(obpt, BLX_INST, SIZEOF(BLX_INST) - 1);
				} else
				{
					memcpy(obpt, BX_INST, SIZEOF(BX_INST) - 1);
				}
				obpt += OPSPC;
				fmt_rm();
				break;
			case 0x15:	/* CMP register */
				memcpy(obpt, CMP_INST, SIZEOF(CMP_INST) - 1);
				obpt += OPSPC;
				fmt_rn_rm();
				break;
			case 0x17:	/* CMN register */
				memcpy(obpt, CMN_INST, SIZEOF(CMN_INST) - 1);
				obpt += OPSPC;
				fmt_rn_rm();
				break;
			case 0x19:	/* ORR */
				memcpy(obpt, ORRS_INST, SIZEOF(ORRS_INST) - 1);
				obpt += OPSPC;
				fmt_rd_rn_rm();
				break;
			case 0x1a:	/* MOV register or LSR */
				if (0x20 == (0x70 & ains))
				{
					/* It's an LSR immediate */
					memcpy(obpt, LSR_INST, SIZEOF(LSR_INST) - 1);
					obpt += OPSPC;
					fmt_rd_rm_shift_imm5();

				} else
				{
					/* It's a MOV register */
					memcpy(obpt, MOV_INST, SIZEOF(MOV_INST) - 1);
					obpt += OPSPC;
					fmt_rd_rm();
				}
				break;
			case 0x24:	/* SUB immediate */
				memcpy(obpt, SUB_INST, SIZEOF(SUB_INST) - 1);
				obpt += OPSPC;
				fmt_rd_rn_shift_imm12();
				break;
			case 0x28:	/* ADD immediate */
				memcpy(obpt, ADD_INST, SIZEOF(ADD_INST) - 1);
				obpt += OPSPC;
				fmt_rd_rn_shift_imm12();
				break;
			case 0x30:	/* MOVW */
				memcpy(obpt, MOVW_INST, SIZEOF(MOVW_INST) - 1);
				obpt += OPSPC;
				fmt_rd_raw_imm16();
				break;
			case 0x31:	/* TST */
				memcpy(obpt, TST_INST, SIZEOF(TST_INST) - 1);
				obpt += OPSPC;
				fmt_rn_shift_imm12();
				break;
			case 0x32:	/* NOP */
				memcpy(obpt, NOP_INST, SIZEOF(NOP_INST) - 1);
				break;
			case 0x34:	/* MOVT */
				memcpy(obpt, MOVT_INST, SIZEOF(MOVT_INST) - 1);
				obpt += OPSPC;
				fmt_rd_raw_imm16();
				break;
			case 0x35:	/* CMP immediate */
				memcpy(obpt, CMP_INST, SIZEOF(CMP_INST) - 1);
				obpt += OPSPC;
				fmt_rn_shift_imm12();
				break;
			case 0x37:	/* CMN immediate */
				memcpy(obpt, CMN_INST, SIZEOF(CMN_INST) - 1);
				obpt += OPSPC;
				fmt_rn_shift_imm12();
				break;
			case 0x3a:	/* MOV immediate */
				memcpy(obpt, MOV_INST, SIZEOF(MOV_INST) - 1);
				obpt += OPSPC;
				fmt_rd_shift_imm12();
				break;
			case 0x3e:	/* MVN immediate */
				memcpy(obpt, MVN_INST, SIZEOF(MOV_INST) - 1);
				obpt += OPSPC;
				fmt_rd_shift_imm12();
				break;
			case 0x50:	/* STR -4095 <= imm12 <= 0 */
			case 0x58:	/* STR  4095 >= imm12 >= 0 */
				memcpy(obpt, STR_INST, SIZEOF(STR_INST) - 1);
				obpt += OPSPC;
				fmt_rt_rn_raw_imm12();
				break;
			case 0x51:	/* LDR -4095 <= imm12 <= 0 */
			case 0x59:	/* LDR  4095 >= imm12 >= 0 */
				if ((0xe51f0000 == (ains & 0xffff0000)) && (0xea000000 == code_buf[instindx + 1]))
				{
					/* it's a "ldr reg,[pc], b pc, constant" construct */
					load_const = 1;
				}
				memcpy(obpt, LDR_INST, SIZEOF(LDR_INST) - 1);
				obpt += OPSPC;
				fmt_rt_rn_raw_imm12();
				break;
			case 0x5d:	/* PLD [reg, #imm12]   0 <= imm12 <= 4095 */
				memcpy(obpt, PLD_INST, SIZEOF(PLD_INST) - 1);
				obpt += OPSPC;
				*obpt++ = '[';
				fmt_rn_raw_imm12();
				*obpt++ = ']';
				break;
			case 0x8b:	/* POP {reg} --> LDR reg,[sp, #4]!   */
				memcpy(obpt, POP_INST, SIZEOF(POP_INST) - 1);
				obpt += OPSPC;
				*obpt++ = '{';
				fmt_registers();
				*obpt++ = '}';
				break;
			case 0x92:	/* PUSH {reg} --> STR reg,[sp, #-4]!   */
				memcpy(obpt, PUSH_INST, SIZEOF(PUSH_INST) - 1);
				obpt += OPSPC;
				*obpt++ = '{';
				fmt_registers();
				*obpt++ = '}';
				break;
			case 0xca:	/* VSTMIA reg!, {d0-dx}   */
				memcpy(obpt, VSTMIA_INST, SIZEOF(VSTMIA_INST) - 1);
				obpt += OPSPC;
				fmt_rn();
				*obpt++ = '!';
				*obpt++ = ',';
				obpt++;
				*obpt++ = '{';
				fmt_sregs();
				*obpt++ = '}';
				break;
			case 0xcb:	/* VLDMIA reg!, {d0-dx}   */
				memcpy(obpt, VLDMIA_INST, SIZEOF(VLDMIA_INST) - 1);
				obpt += OPSPC;
				fmt_rn();
				*obpt++ = '!';
				*obpt++ = ',';
				obpt++;
				*obpt++ = '{';
				fmt_sregs();
				*obpt++ = '}';
				break;
			default:	/* Should be a branch */
				switch(GET_OPCODE(ains) & 0xf0)
				{
					case 0xb0:	/* BL */
						memcpy(obpt, BL_INST, SIZEOF(BL_INST) - 1);
						obpt += OPSPC;
						fmt_brdisp();
						break;

					case 0xa0:	/* The other branches */
						switch(GET_COND(ains))
						{
							case ARM_COND_EQ:
								memcpy(obpt, BEQ_COND, SIZEOF(BEQ_COND) - 1);
								break;
							case ARM_COND_NE:
								memcpy(obpt, BNE_COND, SIZEOF(BNE_COND) - 1);
								break;
							case ARM_COND_GE:
								memcpy(obpt, BGE_COND, SIZEOF(BGE_COND) - 1);
								break;
							case ARM_COND_LT:
								memcpy(obpt, BLT_COND, SIZEOF(BLT_COND) - 1);
								break;
							case ARM_COND_GT:
								memcpy(obpt, BGT_COND, SIZEOF(BGT_COND) - 1);
								break;
							case ARM_COND_LE:
								memcpy(obpt, BLE_COND, SIZEOF(BLE_COND) - 1);
								break;
							case ARM_COND_ALWAYS:
								load_const += (1 == load_const) ? 1 : 0;
								memcpy(obpt, B_INST, SIZEOF(B_INST) - 1);
								break;
							default:
								GTMASSERT;
						}
						obpt += OPSPC;
						fmt_brdisp();
						break;
					default:
						memcpy(obpt, INV_INST, SIZEOF(INV_INST) - 1);
						obpt += SIZEOF(INV_INST);
						fmt_ains();
						break;
				}
				break;
		}
		emit_eoi();
	}
}
#endif

