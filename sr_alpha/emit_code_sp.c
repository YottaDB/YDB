/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "cgp.h"
#include "compiler.h"
#include <rtnhdr.h>
#include "list_file.h"
#include <emit_code.h>

GBLREF uint4		code_buf[];	/* Instruction buffer */
GBLREF int		code_idx;	/* Index into code_buf */
GBLREF char		cg_phase;	/* Current compiler phase */
GBLREF int4		curr_addr;
#ifdef DEBUG
GBLREF unsigned char    *obpt;		/* output buffer index */
GBLREF unsigned char    outbuf[];	/* assembly language output buffer */
static unsigned int	ains;		/* assembler instruction (binary) */
#endif


/* Used by emit_base_offset to extract offset parts */
int	alpha_adjusted_upper(int offset)
{
	int	upper;

	upper = (offset >> 16) & 0xFFFF;
	if (offset & 0x8000)
		upper = (upper + 1) & 0xFFFF;

	return upper;
}


void	emit_base_offset(int base, int offset)
{
	/* NOTE: emit_base_offset does not advance past its last
	   generated instruction because that instruction is
	   incomplete; it contains only a base and offset -- the
	   rt and opcode field are left empty for use by the caller. */
	int	upper, low, source;

	switch (cg_phase)
	{
#ifdef DEBUG
		case CGP_ASSEMBLY:
#endif
		case CGP_ADDR_OPT:
		case CGP_APPROX_ADDR:
		case CGP_MACHINE:
			assert(base >= 0  &&  base <= 31);
			source = base;
			upper = alpha_adjusted_upper(offset);

			if (0 != upper)
			{
				code_buf[code_idx++] = ALPHA_INS_LDAH
					| (GTM_REG_CODEGEN_TEMP << ALPHA_SHIFT_RA)
					| (source               << ALPHA_SHIFT_RB)
					| (upper & ALPHA_MASK_DISP) << ALPHA_SHIFT_DISP;
				source = GTM_REG_CODEGEN_TEMP;
			}
			low = offset & 0xFFFF;
			code_buf[code_idx] = source << ALPHA_SHIFT_RB
				| (low & ALPHA_MASK_DISP) << ALPHA_SHIFT_DISP;
			break;
		default:
			GTMASSERT;
	}
}


#ifdef DEBUG
void fmt_ra()
{
	*obpt++ = 'r';
	obpt = i2asc(obpt, GET_RA(ains));
}
void fmt_ra_rb()
{
	fmt_ra();
	*obpt++ = ',';
	*obpt++;
	*obpt++ = 'r';
	obpt = i2asc(obpt, GET_RB(ains));
}
void fmt_ra_rb_rc()
{
	fmt_ra_rb();
	*obpt++ = ',';
	*obpt++;
	*obpt++ = 'r';
	obpt = i2asc(obpt, GET_RC(ains));
}
void fmt_ra_mem()
{
	fmt_ra();
	*obpt++ = ',';
	obpt++;
	*obpt++ = '0';
	*obpt++ = 'x';
	obpt += i2hex_nofill(GET_MEMDISP(ains), obpt, 6);
	*obpt++ = '(';
	*obpt++ = 'r';
	obpt = i2asc(obpt, GET_RB(ains));
	*obpt++ = ')';
}
void fmt_ra_brdisp()
{
	fmt_ra();
	*obpt++ = ',';
	obpt++;
	*obpt++ = '0';
	*obpt++ = 'x';
	obpt += i2hex_nofill(GET_BRDISP(ains) * 4, obpt, 6);
}
void 	format_machine_inst(void)
{
	int	instindx;

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
		switch(GET_OPCODE(ains))
		{
			case 0x8:
				memcpy(obpt, LDA_INST, SIZEOF(LDA_INST) - 1);
				obpt += OPSPC;
				fmt_ra_mem();
				break;
			case 0x9:
				memcpy(obpt, LDAH_INST, SIZEOF(LDAH_INST) - 1);
				obpt += OPSPC;
				fmt_ra_mem();
				break;
			case 0x10:
			/* Note opcodes 0x10, 0x11, have overlapping functions but none that we generate
			   so we can combine their disassembly.
			*/
			case 0x11:
				switch(GET_FUNC(ains))
				{
					case 0x0:	/* main opcode 0x10 */
						memcpy(obpt, ADDL_INST, SIZEOF(ADDL_INST) - 1);
						break;
					case 0x9:	/* main opcode 0x10 */
						memcpy(obpt, SUBL_INST, SIZEOF(SUBL_INST) - 1);
						break;
					case 0x29:	/* main opcode 0x10 */
						memcpy(obpt, SUBQ_INST, SIZEOF(SUBQ_INST) - 1);
						break;
					case 0x20:	/* main opcode 0x11 */
						memcpy(obpt, BIS_INST, SIZEOF(BIS_INST) - 1);
						break;
					default:
						GTMASSERT;
				}
				obpt += OPSPC;
				fmt_ra_rb_rc();
				break;
			case 0x1a:
				switch(GET_MEMDISP(ains) & 0x3)
				{
					case 0x0:
						memcpy(obpt, JMP_INST, SIZEOF(JMP_INST) - 1);
						break;
					case 0x1:
						memcpy(obpt, JSR_INST, SIZEOF(JSR_INST) - 1);
						break;
					case 0x2:
						memcpy(obpt, RET_INST, SIZEOF(RET_INST) - 1);
						break;
					default:
						GTMASSERT;
				}
				obpt += OPSPC;
				fmt_ra_rb();
				break;
			case 0x28:
				memcpy(obpt, LDL_INST, SIZEOF(LDL_INST) - 1);
				obpt += OPSPC;
				fmt_ra_mem();
				break;
			case 0x29:
				memcpy(obpt, LDQ_INST, SIZEOF(LDQ_INST) - 1);
				obpt += OPSPC;
				fmt_ra_mem();
				break;
			case 0x2c:
				memcpy(obpt, STL_INST, SIZEOF(STL_INST) - 1);
				obpt += OPSPC;
				fmt_ra_mem();
				break;
			case 0x2d:
				memcpy(obpt, STQ_INST, SIZEOF(STQ_INST) - 1);
				obpt += OPSPC;
				fmt_ra_mem();
				break;
			case 0x30:
				memcpy(obpt, BR_INST, SIZEOF(BR_INST) - 1);
				obpt += OPSPC;
				fmt_ra_brdisp();
				break;
			case 0x34:
				memcpy(obpt, BSR_INST, SIZEOF(BSR_INST) - 1);
				obpt += OPSPC;
				fmt_ra_brdisp();
				break;
			case 0x38:
				memcpy(obpt, BLBC_INST, SIZEOF(BLBC_INST) - 1);
				obpt += OPSPC;
				fmt_ra_brdisp();
				break;
			case 0x39:
				memcpy(obpt, BEQ_INST, SIZEOF(BEQ_INST) - 1);
				obpt += OPSPC;
				fmt_ra_brdisp();
				break;
			case 0x3a:
				memcpy(obpt, BLT_INST, SIZEOF(BLT_INST) - 1);
				obpt += OPSPC;
				fmt_ra_brdisp();
				break;
			case 0x3b:
				memcpy(obpt, BLE_INST, SIZEOF(BLE_INST) - 1);
				obpt += OPSPC;
				fmt_ra_brdisp();
				break;
			case 0x3c:
				memcpy(obpt, BLBS_INST, SIZEOF(BLBS_INST) - 1);
				obpt += OPSPC;
				fmt_ra_brdisp();
				break;
			case 0x3d:
				memcpy(obpt, BNE_INST, SIZEOF(BNE_INST) - 1);
				obpt += OPSPC;
				fmt_ra_brdisp();
				break;
			case 0x3e:
				memcpy(obpt, BGE_INST, SIZEOF(BGE_INST) - 1);
				obpt += OPSPC;
				fmt_ra_brdisp();
				break;
			case 0x3f:
				memcpy(obpt, BGT_INST, SIZEOF(BGT_INST) - 1);
				obpt += OPSPC;
				fmt_ra_brdisp();
				break;
			default: /* Not an instruction but a constant */
				memcpy(obpt, CONSTANT, SIZEOF(CONSTANT) - 1);
				obpt += SIZEOF(CONSTANT) - 1;
				i2hex(ains, obpt, 8);
				obpt += 8;
		}
		emit_eoi();
	}
}
#endif
