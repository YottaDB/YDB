/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "compiler.h"
#include <rtnhdr.h>
#include "stack_frame.h"
#include "opcode.h"
#include "xfer_enum.h"
#include "mdq.h"
#include "vxi.h"
#include "vxt.h"
#include "cgp.h"
#include "obj_gen.h"
#include "i386.h"
#include "obj_file.h"
#include <emit_code.h>
#include "hashtab_mname.h"
#include "stddef.h"


#define BUFFERED_CODE_SIZE 50
#define LONG_JUMP_OFFSET (0x7ffffffc)

#define XFER_BYTE_INST_SIZE 3
#define XFER_LONG_INST_SIZE 6
#define BRB_INST_SIZE 2
#define JMP_LONG_INST_SIZE 5
/* index in ttt from start of call[sp] and forlcldo to xfer_table index */
#define CALL_4LCLDO_XFER 2

typedef enum
{
	CLEAR,
	COMPARE,
	INCREMENT,
	JUMP,
	LOAD,
	LOAD_ADDRESS,
	PUSH,
	PUSH_ADDRESS,
	STORE,
	TEST
} generic_op;

void emit_pcrel(generic_op op, unsigned char use_reg);
void emit_trip(generic_op op, oprtype *opr, bool val_output, unsigned char use_reg);
void emit_op_base_offset(generic_op op, short base_reg, int offset, short use_reg);
void emit_op_alit (generic_op op, unsigned char use_reg);
void emit_jmp(short vax_in, short **instp);
unsigned char i386_reg(unsigned char vax_reg);

union
{
	ModR_M		modrm;
	unsigned char	byte;
} modrm_byte;

union
{
	SIB		sib;
	unsigned char	byte;
} sib_byte;

LITREF octabstruct	oc_tab[];	/* op-code table */
LITREF short		ttt[];		/* triple templates */

static unsigned char code_buf[BUFFERED_CODE_SIZE];
static unsigned short code_idx;
static int4 jmp_offset, code_reference;
static int force_32;
static int call_4lcldo_variant;		/* used in emit_jmp for call[sp] and forlcldo */

GBLREF int4		curr_addr;
GBLREF char		cg_phase;	/* code generation phase */
GBLDEF uint4	txtrel_cnt;	/* count of text relocation records */

/* its referenced in ind_code.c */
GBLDEF int              calculated_code_size, generated_code_size;

error_def(ERR_UNIMPLOP);
error_def(ERR_MAXARGCNT);

void trip_gen(triple *ct)
{
	oprtype		**sopr, *opr;	/* triple operand */
	oprtype		*saved_opr[MAX_ARGS];
	unsigned short	oct;
	short		tp;	/* template pointer */
	short		*tsp;	/* template short pointer */
	triple		*ttp;	/* temp triple pointer */
	short		irep_index;
	oprtype		*irep_opr;
	short		*repl, repcnt;	/* temp irep ptr */
	int4		off;

	tp = ttt[ct->opcode];
	if (tp <= 0)
	{
		stx_error(ERR_UNIMPLOP);
		return;
	}

	code_idx = 0;
	code_reference = ct->rtaddr;
	oct = oc_tab[ct->opcode].octype;
	sopr = &saved_opr[0];
	*sopr++ = &ct->destination;
	for (ttp = ct, opr = ttp->operand ; opr < ARRAYTOP(ttp->operand); )
	{
		if (opr->oprclass)
		{
			if (opr->oprclass == TRIP_REF && opr->oprval.tref->opcode == OC_PARAMETER)
			{
				ttp = opr->oprval.tref;
				opr = ttp->operand;
				continue;
			}
			*sopr++ = opr;
			if (sopr >= ARRAYTOP(saved_opr))	/* user-visible max args is MAX_ARGS - 3 */
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_MAXARGCNT, 1, MAX_ARGS - 3);
		}
		opr++;
	}
	*sopr=0;
	jmp_offset = 0;
	call_4lcldo_variant = 0;
	if (oct & OCT_JUMP || ct->opcode == OC_LDADDR || ct->opcode == OC_FORLOOP)
	{
		if (ct->operand[0].oprval.tref->rtaddr == 0)		/* forward reference */
		{
			jmp_offset = LONG_JUMP_OFFSET;
			assert(cg_phase == CGP_APPROX_ADDR);
		}
		else
			jmp_offset = ct->operand[0].oprval.tref->rtaddr - ct->rtaddr;

		switch (ct->opcode)
		{
		case OC_CALL:
		case OC_FORLCLDO:
		case OC_CALLSP:
/*  Changes to emit_xfer, emit_base_offset, or emit_jmp may require changes
	here since we try to predict how big the call into the xfer_table
	and the following jump will be.
	There is also an assumption that both the word and long variants
	of the opcode will be followed by a jmp with 32 bit offset while
	the -BYTE variants will be followed by a BRB with an 8 bit offset.
*/
			tsp = (short *)&ttt[ttt[tp]];
			if (-128 <= tsp[CALL_4LCLDO_XFER] && 127 >= tsp[CALL_4LCLDO_XFER])
				off = jmp_offset - XFER_BYTE_INST_SIZE;
			else
				off = jmp_offset - XFER_LONG_INST_SIZE;
			if (-128 <= (off - BRB_INST_SIZE) && 127 >= (off - BRB_INST_SIZE))
				call_4lcldo_variant = BRB_INST_SIZE;	/* used by emit_jmp */
			else
			{
			    call_4lcldo_variant = JMP_LONG_INST_SIZE;	/* used by emit_jmp */
			    tsp = (short *)&ttt[ttt[tp + 1]];
			    if (-128 <= tsp[CALL_4LCLDO_XFER] && 127 >= tsp[CALL_4LCLDO_XFER])
			        off = jmp_offset - XFER_BYTE_INST_SIZE;
			    else
				off = jmp_offset - XFER_LONG_INST_SIZE;
			    if (-32768 > (off - JMP_LONG_INST_SIZE) &&
				  32767 < (off - JMP_LONG_INST_SIZE))
				tsp = (short *)&ttt[ttt[tp + 2]];
			}
			break;
		case OC_JMP:
		case OC_JMPEQU:
		case OC_JMPGEQ:
		case OC_JMPGTR:
		case OC_JMPLEQ:
		case OC_JMPNEQ:
		case OC_JMPLSS:
		case OC_JMPTSET:
		case OC_JMPTCLR:
		case OC_LDADDR:
		case OC_FORLOOP:
			tsp = (short *)&ttt[ttt[tp]];
			break;
		default:
			GTMASSERT;
			break;
		}
	}
	else if (oct & OCT_COERCE)
	{
		switch  (oc_tab[ct->operand[0].oprval.tref->opcode].octype & (OCT_VALUE | OCT_BOOL))
		{
		case OCT_MVAL:
			tp = ttt[tp];
			break;
		case OCT_MINT:
			tp = ttt[tp + 3];
			break;
		case OCT_BOOL:
			tp = ttt[tp + 4];
			break;
		default:
			GTMASSERT;
			break;
		}
		tsp = (short *)&ttt[tp];
	}
	else
		tsp = (short *)&ttt[tp];

	for (; *tsp != VXT_END; )
	{
		if (*tsp == VXT_IREPAB || *tsp == VXT_IREPL)
		{
			repl = tsp;
			repl += 2;
			repcnt = *repl++;
			assert(repcnt != 1);
			for (irep_index = repcnt, irep_opr = &ct->operand[1]; irep_index > 2; --irep_index)
			{
				assert (irep_opr->oprclass == TRIP_REF);
				irep_opr = &irep_opr->oprval.tref->operand[1];
			}

			if (irep_opr->oprclass == TRIP_REF)
			{
				repl = tsp;
				do
				{
					tsp = repl;
					tsp = emit_vax_inst(tsp, &saved_opr[0], --sopr);
				} while (sopr > &saved_opr[repcnt]);
			}
			else
			{
				sopr = &saved_opr[repcnt];
				tsp = repl;
			}
		}
		else
		{
			assert(*tsp > 0 && *tsp <= 511);
			tsp = emit_vax_inst(tsp, &saved_opr[0], sopr);
		}/* else */
	}/* for */
}


short *emit_vax_inst(short *inst, oprtype **fst_opr, oprtype **lst_opr)
     /* fst_opr and lst_opr are triple operands */
{
	short	sav_in;
	bool	oc_int;
	int4	cnt;
	oprtype *opr;
	triple	*ct;

	code_idx = 0;
	force_32 = 0;

	switch  (cg_phase)
	{
	case CGP_ADDR_OPT:
	case CGP_APPROX_ADDR:
	case CGP_MACHINE:
		switch ((sav_in = *inst++))
		{
			case VXI_BEQL:
			case VXI_BGEQ:
			case VXI_BGTR:
			case VXI_BLEQ:
			case VXI_BLSS:
			case VXI_BNEQ:
			case VXI_BRB:
			case VXI_BRW:
				emit_jmp(sav_in, &inst);
				break;
			case VXI_BLBC:
			case VXI_BLBS:
				assert (*inst == VXT_REG);
				inst++;
				inst++;
				emit_xfer(4*xf_dt_get);
				code_buf[code_idx++] = I386_INS_CMP_eAX_Iv;
				*((int4 *)&code_buf[code_idx]) = 0;
				code_idx += SIZEOF(int4);
				if (sav_in == VXI_BLBC)
					emit_jmp(VXI_BEQL, &inst);
				else
				{
					assert (sav_in == VXI_BLBS);
					emit_jmp(VXI_BNEQ, &inst);
				}
				break;
			case VXI_BICB2:
			case VXI_BISB2:
				assert (*inst == VXT_LIT);
				inst++;
				assert (*inst == 1);
				inst++;
				assert (*inst == VXT_REG);
				inst++;
				inst++;
				if (sav_in == VXI_BICB2)
					emit_xfer(4*xf_dt_false);
				else
				{
					assert (sav_in == VXI_BISB2);
					emit_xfer(4*xf_dt_true);
				}
				break;
			case VXI_CALLS:
				oc_int = TRUE;
				if (*inst == VXT_LIT)
				{
					inst++;
					cnt = (int4) *inst++;
				}
				else
				{
					assert(*inst == VXT_VAL);
					inst++;
					opr = *(fst_opr + *inst);
					assert (opr->oprclass == TRIP_REF);
					ct = opr->oprval.tref;
					if (ct->destination.oprclass)
					{
						opr = &ct->destination;
					}
					if (opr->oprclass == TRIP_REF)
					{
						assert(ct->opcode == OC_ILIT);
						cnt = ct->operand[0].oprval.ilit;
						if (cnt >= -128  &&  cnt <= 127)
						{
							code_buf[code_idx++] = I386_INS_PUSH_Ib;
							code_buf[code_idx++] = cnt & 0xff;
						}
						else
						{
							code_buf[code_idx++] = I386_INS_PUSH_Iv;
							*((int4 *)&code_buf[code_idx]) = cnt;
							code_idx += SIZEOF(int4);
						}
						cnt++;
						inst++;
					}
					else
					{
						assert(opr->oprclass == TINT_REF);
						oc_int = FALSE;
						opr = *(fst_opr + *inst++);
						emit_trip(PUSH, opr, TRUE, 0);
					}
				}
				assert (*inst == VXT_XFER);
				inst++;
				emit_xfer(*inst++);
				if (oc_int)
				{
					if (cnt)
					{
						code_buf[code_idx++] = I386_INS_LEA_Gv_M;
						emit_base_offset(I386_REG_ESP, I386_REG_ESP, 4*cnt);
					}
				}
				else
				{
					emit_trip(LOAD, opr, TRUE, I386_REG_EDX);

					code_buf[code_idx++] = I386_INS_LEA_Gv_M;
					emit_base_offset(I386_REG_ESP, I386_REG_ESP, 4);
				}
				break;
			case VXI_CLRL:
				assert (*inst == VXT_VAL);
				inst++;
				emit_trip(CLEAR, *(fst_opr + *inst++), TRUE, 0);
				break;
			case VXI_CMPL:
				assert (*inst == VXT_VAL);
				inst++;
				emit_trip(LOAD, *(fst_opr + *inst++), TRUE, I386_REG_EDX);
				assert (*inst == VXT_VAL);
				inst++;
				emit_trip(COMPARE, *(fst_opr + *inst++), TRUE, I386_REG_EDX);
				break;
			case VXI_INCL:
				assert (*inst == VXT_VAL);
				inst++;
				emit_trip(INCREMENT, *(fst_opr + *inst++), TRUE, 0);
				break;
			case VXI_JMP:
				if (*inst == VXT_VAL)
				{
					inst++;
					emit_trip(JUMP, *(fst_opr + *inst++), FALSE, 0);
				}
				else
				{
					emit_jmp(sav_in, &inst);
				}
				break;
			case VXI_JSB:
				assert (*inst == VXT_XFER);
				inst++;
				emit_xfer(*inst++);
				break;
			case VXI_MOVAB:
				if (*inst == VXT_JMP)
				{
					inst += 2;
					emit_pcrel(LOAD_ADDRESS, I386_REG_EAX);
					assert (*inst == VXT_ADDR);
					inst++;
					emit_trip(STORE, *(fst_opr + *inst++), FALSE, I386_REG_EAX);
				}
				else if (*inst == VXT_ADDR || *inst == VXT_VAL)
				{
					bool	addr;
					unsigned char reg;
					short	save_inst;

					addr = (*inst == VXT_VAL);
					inst++;
					save_inst = *inst++;
					assert (*inst == VXT_REG);
					inst++;
					reg = ((*inst++ & 0x01) ? I386_REG_EDX : I386_REG_EAX); /* r0 and r1 are only ones used */
					emit_trip(LOAD_ADDRESS, *(fst_opr + save_inst), addr, reg);
				}
				else
					GTMASSERT;
				break;
			case VXI_MOVC3:
				assert (*inst == VXT_LIT);
				inst += 2;
				assert(*inst == VXT_VAL);
				inst++;
				code_buf[code_idx++] = I386_INS_PUSH_eSI;
				code_buf[code_idx++] = I386_INS_PUSH_eDI;

				emit_trip(LOAD_ADDRESS, *(fst_opr + *inst++), TRUE, I386_REG_ECX);

				assert(*inst == VXT_VAL);
				inst++;
				emit_trip(LOAD_ADDRESS, *(fst_opr + *inst++), TRUE, I386_REG_EDI);

				code_buf[code_idx++] = I386_INS_MOV_Gv_Ev;
				modrm_byte.modrm.reg_opcode = I386_REG_ESI;
				modrm_byte.modrm.mod = I386_MOD32_REGISTER;
				modrm_byte.modrm.r_m = I386_REG_ECX;
				code_buf[code_idx++] = modrm_byte.byte;

				code_buf[code_idx++] = I386_INS_MOV_eCX;
				*((int4 *)&code_buf[code_idx]) = (int4)SIZEOF(mval);
				code_idx += SIZEOF(int4);

				code_buf[code_idx++] = I386_INS_REP_E_Prefix;
				code_buf[code_idx++] = I386_INS_MOVSB_Xb_Yb;

				code_buf[code_idx++] = I386_INS_POP_eDI;
				code_buf[code_idx++] = I386_INS_POP_eSI;
				break;
			case VXI_MOVL:
				if (*inst == VXT_REG)
				{
					inst++;
					if (*inst > 0x5f)	/* OC_CURRHD */  /* any mode >= 6 (deferred), any register */
					{
						inst++;
						assert (*inst == VXT_ADDR);
						inst++;

						emit_xfer(4*xf_get_msf);
						emit_op_base_offset(LOAD, I386_REG_EAX, 0, I386_REG_EAX);
						emit_trip(STORE, *(fst_opr + *inst++), FALSE, I386_REG_EAX);
					}
					else
					{
						bool addr;

						assert (*inst == 0x50);  /* register mode: R0 */
						inst++;
						if (*inst == VXT_VAL || *inst == VXT_ADDR)
						{
							addr = (*inst == VXT_VAL);
							inst++;
							emit_trip(STORE, *(fst_opr + *inst++), addr, I386_REG_EAX);
						}
						else if (*inst == VXT_REG)
						{
							unsigned char	reg;

							inst++;
							if ((*inst & 0x0f) == 10)	/* VAX $TEST */
							{
								code_buf[code_idx++] = I386_INS_PUSH_eAX;
								emit_xfer(4*xf_dt_store);
								code_buf[code_idx++] = I386_INS_POP_eAX;
							}
							else
							{
								code_buf[code_idx++] = I386_INS_MOV_Ev_Gv;
								modrm_byte.modrm.reg_opcode = I386_REG_EAX;
								modrm_byte.modrm.mod = I386_MOD32_REGISTER;
								modrm_byte.modrm.r_m = i386_reg(*inst);
								code_buf[code_idx++] = modrm_byte.byte;
							}
							inst++;
						}
						else
							GTMASSERT;
					}
				}
				else if (*inst == VXT_VAL)
				{
					inst++;
					emit_trip(LOAD, *(fst_opr + *inst++), TRUE, I386_REG_EDX);
					assert (*inst == VXT_REG);
					inst++;
					assert (*inst == 0x51);  /* register mode: R1 */
					inst++;
				}
				else
					GTMASSERT;
				break;
			case VXT_IREPAB:
				assert (*inst == VXT_VAL);
				inst += 2;
				emit_trip(PUSH_ADDRESS, *lst_opr, TRUE, 0);
				break;
			case VXI_PUSHAB:
				if (*inst == VXT_JMP)
				{
					inst += 2;
					emit_pcrel(PUSH_ADDRESS, 0);
				}
				else if (*inst == VXT_VAL)
				{
					inst++;
					emit_trip(PUSH_ADDRESS, *(fst_opr + *inst++), TRUE, 0);
				}
				else
					GTMASSERT;
				break;
			case VXT_IREPL:
				assert (*inst == VXT_VAL);
				inst += 2;
				emit_trip(PUSH, *lst_opr, TRUE, 0);
				break;
			case VXI_PUSHL:
				if (*inst == VXT_LIT)
				{
					int4	lit;

					inst++;
					lit = *inst++;
					if (lit >= -128  &&  lit <= 127)
					{
						code_buf[code_idx++] = I386_INS_PUSH_Ib;
						code_buf[code_idx++] = lit & 0xff;
					}
					else
					{
						code_buf[code_idx++] = I386_INS_PUSH_Iv;
						*((int4 *)&code_buf[code_idx]) = lit;
						code_idx += SIZEOF(int4);
					}
				}
				else if (*inst == VXT_ADDR)
				{
					inst++;
					emit_trip(PUSH, *(fst_opr + *inst++), FALSE, 0);
				}
				else if (*inst == VXT_VAL)
				{
					inst++;
					emit_trip(PUSH, *(fst_opr + *inst++), TRUE, 0);
				}
				else
					GTMASSERT;
				break;
			case VXI_TSTL:
				if (*inst == VXT_VAL)
				{
				  inst++;
				  emit_trip(TEST, *(fst_opr + *inst++), TRUE, 0);
				}
				else if (VXT_REG == *inst)
				{
				    inst++;
				    code_buf[code_idx++] = I386_INS_CMP_eAX_Iv;
				    assert(I386_REG_EAX == i386_reg(*inst));	/* VAX R0 */
				    inst++;
				    *((int4 *)&code_buf[code_idx]) = 0;	/* 32 bit immediate 0 */
				    code_idx += SIZEOF(int4);
				}
				else
				  GTMASSERT;
				break;
			default:
				GTMASSERT;
		}
		break;
	default:
		GTMASSERT;
		break;
	}
	assert (code_idx < BUFFERED_CODE_SIZE);
	if (cg_phase == CGP_MACHINE)
	{
         	generated_code_size += code_idx;
		emit_immed ((char *)&code_buf[0], SIZEOF(unsigned char) * code_idx);
	} else if (cg_phase != CGP_ASSEMBLY)
	{
	        if (cg_phase == CGP_APPROX_ADDR)
		{
		      calculated_code_size += code_idx;
		}
		curr_addr += SIZEOF(unsigned char) * code_idx;
	}
	code_reference += SIZEOF(unsigned char) * code_idx;
	jmp_offset -= SIZEOF(unsigned char) * code_idx;
	return inst;
}

/*  Changes here or emit_xfer may require changes in trip_gen case for
	OC_CALL[SP] and FORLCLDO
*/
void emit_jmp(short vax_in, short **instp)
{

	assert (jmp_offset != 0);
	jmp_offset -= code_idx * SIZEOF(code_buf[0]);	/* size of this particular instruction */

	assert (**instp == VXT_JMP);
	*instp += 1;
	assert (**instp == 1);
	*instp += 1;
	if (jmp_offset == 0)
	{
		code_buf[code_idx++] = I386_INS_NOP__;
	}
	else if ((jmp_offset - 2) >= -128  &&  (jmp_offset - 2) <= 127 &&
			JMP_LONG_INST_SIZE != call_4lcldo_variant)
	{
		jmp_offset -= 2;
		switch  (vax_in)
		{
		case VXI_BEQL:
			code_buf[code_idx++] = I386_INS_JZ_Jb;
			break;
		case VXI_BGEQ:
			code_buf[code_idx++] = I386_INS_JNL_Jb;
			break;
		case VXI_BGTR:
			code_buf[code_idx++] = I386_INS_JNLE_Jb;
			break;
		case VXI_BLEQ:
			code_buf[code_idx++] = I386_INS_JLE_Jb;
			break;
		case VXI_BLSS:
			code_buf[code_idx++] = I386_INS_JL_Jb;
			break;
		case VXI_BNEQ:
			code_buf[code_idx++] = I386_INS_JNZ_Jb;
			break;
		case VXI_BRB:
		case VXI_BRW:
		case VXI_JMP:
			assert(0 == call_4lcldo_variant || BRB_INST_SIZE == call_4lcldo_variant);
			code_buf[code_idx++] = I386_INS_JMP_Jb;
			break;
		default:
			GTMASSERT;
			break;
		}
		code_buf[code_idx++] = jmp_offset & 0xff;
	}
	else
	{
		if (vax_in == VXI_BRB  ||  vax_in == VXI_BRW  ||  vax_in == VXI_JMP)
		{
			assert(0 == call_4lcldo_variant || JMP_LONG_INST_SIZE == call_4lcldo_variant);
			jmp_offset -= SIZEOF(int4) + 1;
			code_buf[code_idx++] = I386_INS_JMP_Jv;
		}
		else
		{
			jmp_offset -= SIZEOF(int4) + 2;
			code_buf[code_idx++] = I386_INS_Two_Byte_Escape_Prefix;
			switch  (vax_in)
			{
			case VXI_BEQL:
				code_buf[code_idx++] = I386_INS_JZ_Jv;
				break;
			case VXI_BGEQ:
				code_buf[code_idx++] = I386_INS_JNL_Jv;
				break;
			case VXI_BGTR:
				code_buf[code_idx++] = I386_INS_JNLE_Jv;
				break;
			case VXI_BLEQ:
				code_buf[code_idx++] = I386_INS_JLE_Jv;
				break;
			case VXI_BLSS:
				code_buf[code_idx++] = I386_INS_JL_Jv;
				break;
			case VXI_BNEQ:
				code_buf[code_idx++] = I386_INS_JNZ_Jv;
				break;
			default:
				GTMASSERT;
				break;
			}
		}
		*((int4 *)&code_buf[code_idx]) = jmp_offset;
		code_idx += SIZEOF(int4);
	}
}


void emit_pcrel(generic_op op, unsigned char use_reg)
{
	code_buf[code_idx++] = I386_INS_CALL_Jv;
	*((int4 *)&code_buf[code_idx]) = 0;
	code_idx += SIZEOF(int4);

	jmp_offset -= code_idx;
	code_buf[code_idx++] = I386_INS_POP_eAX;

	emit_op_base_offset(op, I386_REG_EAX, jmp_offset, use_reg);
}


GBLREF boolean_t	run_time;
GBLREF int4		sa_temps_offset[];
GBLREF int4		sa_temps[];
LITREF int4		sa_class_sizes[];

void emit_trip(generic_op op, oprtype *opr, bool val_output, unsigned char use_reg)
{
	unsigned char		base_reg, temp_reg;
	int4			offset, literal;
	triple			*ct;

	if (opr->oprclass == TRIP_REF)
	{
		ct = opr->oprval.tref;
		if (ct->destination.oprclass)
		{
			opr = &ct->destination;
		}
		/* else lit or error */
	}

	switch (cg_phase)
	{
	case CGP_ADDR_OPT:
	case CGP_APPROX_ADDR:
		switch (opr->oprclass)
		{
		case TRIP_REF:
			assert(ct->destination.oprclass == 0);
			assert(val_output);
			switch (ct->opcode)
			{
			case OC_LIT:
				if (run_time)
				{
					int4	pc_value_idx;

					switch (op)
					{
					case LOAD_ADDRESS:
						temp_reg = use_reg;
						break;
					case PUSH:
					case PUSH_ADDRESS:
						temp_reg = I386_REG_ECX;
						break;
					default:
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_UNIMPLOP);
						break;
					}
					pc_value_idx = code_idx + 5;
					code_idx += 1 + SIZEOF(int4) + 1;
					emit_addr(0, (int4)ct->operand[0].oprval.mlit->rt_addr, &offset);
					offset -= pc_value_idx;
					force_32 = 1;
					emit_op_base_offset(op, temp_reg, offset, temp_reg);
					force_32 = 0;
				}
				else
				{
					emit_op_alit(op, use_reg);
					code_idx += SIZEOF(int4);
				}
				if (cg_phase == CGP_APPROX_ADDR)
					txtrel_cnt++;
				break;
			case OC_CDLIT:
				if (cg_phase == CGP_APPROX_ADDR)
					define_symbol(GTM_LITERALS, ct->operand[0].oprval.cdlt, 0);
				emit_op_alit(op, use_reg);
				code_idx += SIZEOF(int4);
				break;
			case OC_ILIT:
				literal = ct->operand[0].oprval.ilit;
				switch(op)
				{
				case COMPARE: /* 1byte(opcode) + 1byte(ModR/M) + 4byte(literal) */
					code_idx += 2 + SIZEOF(int4);
					break;
				case LOAD:
					code_idx += 1 + SIZEOF(int4);
					break;
				case PUSH:
					if (literal >= -128  &&  literal <= 127)
						code_idx += 2;
					else
						code_idx += 1 + SIZEOF(int4);
					break;
				default:
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_UNIMPLOP);
					break;
				}
				break;
			default:
				GTMASSERT;
				break;
			}
			break;
		case TINT_REF:
		case TVAL_REF:
			assert(val_output);
			offset = sa_temps_offset[opr->oprclass];
			offset -= (sa_temps[opr->oprclass] - opr->oprval.temp) * sa_class_sizes[opr->oprclass];
			if (offset < 0  &&  offset > 65535)
				GTMASSERT;
			emit_op_base_offset(op, I386_REG_EDI, offset, use_reg);
			break;
		case TCAD_REF:
		case TVAD_REF:
		case TVAR_REF:
			offset = sa_temps_offset[opr->oprclass];
			offset -= (sa_temps[opr->oprclass] - opr->oprval.temp) * sa_class_sizes[opr->oprclass];
			if (offset < 0  &&  offset > 65535)
				GTMASSERT;

			if (opr->oprclass == TVAR_REF)
				base_reg = I386_REG_ESI;
			else
				base_reg = I386_REG_EDI;
			switch (op)
			{
			case JUMP:
				if (val_output)
				{
					code_idx++;
					emit_base_offset(I386_REG_EAX, base_reg, offset);
				}
				code_idx++;
				if (val_output)
					emit_base_offset(I386_INS_JMP_Ev, I386_REG_EAX, 0);
				else
					emit_base_offset(I386_INS_JMP_Ev, base_reg, offset);
				break;
			case LOAD_ADDRESS:
				code_idx++;
				emit_base_offset(use_reg, base_reg, offset);
				if (opr->oprclass == TVAR_REF)
				{
					code_idx++;
					emit_base_offset(use_reg, use_reg, offsetof(ht_ent_mname, value));
				}
				break;
			case PUSH:
				if (!val_output)
				{
					code_idx++;
					emit_base_offset(I386_INS_PUSH_Ev, base_reg, offset);
				}
				else
				{
					code_idx++;
					emit_base_offset(I386_REG_ECX, base_reg, offset);

					code_idx++;
					emit_base_offset(I386_INS_PUSH_Ev, I386_REG_ECX, 0);
				}
				break;
			case PUSH_ADDRESS:
				if (val_output)
				{
					if (opr->oprclass == TVAR_REF)
					{
						code_idx++;
						emit_base_offset(use_reg, base_reg, offset);
						code_idx++;
						emit_base_offset(I386_INS_PUSH_Ev, use_reg, offsetof(ht_ent_mname, value));
					}
					else
					{
						code_idx++;
						emit_base_offset(I386_INS_PUSH_Ev, base_reg, offset);
					}
				}
				else
				{
					code_idx++;
					emit_base_offset(I386_REG_ECX, base_reg, offset);
					code_idx++;
				}
				break;
			case STORE:
				if (val_output)
				{
					if (use_reg == I386_REG_EAX)
						temp_reg = I386_REG_EDX;
					else
						temp_reg = I386_REG_EAX;
					code_idx++;
					emit_base_offset(temp_reg, base_reg, offset);
				}
				code_idx++;
				if (val_output)
					emit_base_offset(use_reg, temp_reg, 0);
				else
					emit_base_offset(use_reg, base_reg, offset);
				break;
			default:
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_UNIMPLOP);
				break;
			}
			break;
		}
		break;
	case CGP_MACHINE:
		switch (opr->oprclass)
		{
		case TRIP_REF:
			assert(ct->destination.oprclass == 0);
			assert(val_output);
			switch (ct->opcode)
			{
			case OC_LIT:
				assert(ct->operand[0].oprclass == MLIT_REF);
				if (run_time)
				{
					int4	pc_value_idx;

					switch(op)
					{
					case LOAD_ADDRESS:
						temp_reg = use_reg;
						break;
					case PUSH:
					case PUSH_ADDRESS:
						temp_reg = I386_REG_ECX;
						break;
					default:
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_UNIMPLOP);
						break;
					}
					code_buf[code_idx++] = I386_INS_CALL_Jv;
					*((int4 *)&code_buf[code_idx]) = 0;
					code_idx += SIZEOF(int4);

					pc_value_idx = code_idx;
					code_buf[code_idx++] = I386_INS_POP_eAX + temp_reg;

					emit_addr(0, (int4)ct->operand[0].oprval.mlit->rt_addr, &offset);
					offset -= pc_value_idx;
					force_32 = 1;
					emit_op_base_offset(op, temp_reg, offset, temp_reg);
					force_32 = 0;
				}
				else
				{
					emit_op_alit(op, use_reg);
					emit_addr(code_reference + (code_idx * SIZEOF(unsigned char)),
						(int4)ct->operand[0].oprval.mlit->rt_addr, (int4 *)&code_buf[code_idx]);
					code_idx += SIZEOF(int4);
				}
				break;
			case OC_CDLIT:
				emit_op_alit(op, use_reg);
				emit_reference(code_reference + (code_idx * SIZEOF(unsigned char)),
					ct->operand[0].oprval.cdlt, (uint4 *)&code_buf[code_idx]);
				code_idx += SIZEOF(int4);
				break;
			case OC_ILIT:
				literal = ct->operand[0].oprval.ilit;
				switch (op)
				{
				case COMPARE: /* cmpl $literal,use_reg - 1byte(opcode) + 1byte(ModR/M) + 4byte(literal) */
					code_buf[code_idx++] = I386_INS_Grp1_Ev_Iv_Prefix;
					modrm_byte.modrm.reg_opcode = I386_INS_CMP__;
					modrm_byte.modrm.mod = I386_MOD32_REGISTER;
					modrm_byte.modrm.r_m = use_reg;
					code_buf[code_idx++] = modrm_byte.byte;
					*((int4 *)&code_buf[code_idx]) = literal;
					code_idx += SIZEOF(int4);
					break;
				case LOAD:
					code_buf[code_idx++] = I386_INS_MOV_eAX + use_reg;
					*((int4 *)&code_buf[code_idx]) = literal;
					code_idx += SIZEOF(int4);
					break;
				case PUSH:
					if (literal >= -128  &&  literal <= 127)
					{
						code_buf[code_idx++] = I386_INS_PUSH_Ib;
						code_buf[code_idx++] = literal & 0xff;
					}
					else
					{
						code_buf[code_idx++] = I386_INS_PUSH_Iv;
						*((int4 *)&code_buf[code_idx]) = literal;
						code_idx += SIZEOF(int4);
					}
					break;
				default:
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_UNIMPLOP);
					break;
				}
				break;
			default:
				GTMASSERT;
				break;
			}
			break;
		case TINT_REF:
		case TVAL_REF:
			assert(val_output);
			offset = sa_temps_offset[opr->oprclass];
			offset -= (sa_temps[opr->oprclass] - opr->oprval.temp) * sa_class_sizes[opr->oprclass];
			if (offset < 0  &&  offset > 65535)
				GTMASSERT;
			emit_op_base_offset(op, I386_REG_EDI, offset, use_reg);
			break;
		case TCAD_REF:
		case TVAD_REF:
		case TVAR_REF:
			offset = sa_temps_offset[opr->oprclass];
			offset -= (sa_temps[opr->oprclass] - opr->oprval.temp) * sa_class_sizes[opr->oprclass];
			if (offset < 0  &&  offset > 65535)
				GTMASSERT;
			if (opr->oprclass == TVAR_REF)
				base_reg = I386_REG_ESI;
			else
				base_reg = I386_REG_EDI;

			switch (op)
			{
			case JUMP:
				assert (use_reg == 0);
				if (val_output)
				{
					code_buf[code_idx++] = I386_INS_MOV_Gv_Ev;
					emit_base_offset(I386_REG_EAX, base_reg, offset);
				}
				code_buf[code_idx++] = I386_INS_Grp5_Prefix;
				if (val_output)
					emit_base_offset(I386_INS_JMP_Ev, I386_REG_EAX, 0);
				else
					emit_base_offset(I386_INS_JMP_Ev, base_reg, offset);
				break;
			case LOAD_ADDRESS:
				if (val_output)
					code_buf[code_idx++] = I386_INS_MOV_Gv_Ev;
				else
					code_buf[code_idx++] = I386_INS_LEA_Gv_M;
				emit_base_offset(use_reg, base_reg, offset);
				if (opr->oprclass == TVAR_REF)
				{
					code_buf[code_idx++] = I386_INS_MOV_Gv_Ev;
					emit_base_offset(use_reg, use_reg, offsetof(ht_ent_mname, value));
				}
				break;
			case PUSH:
				if (val_output)
				{
					code_buf[code_idx++] = I386_INS_MOV_Gv_Ev;
					emit_base_offset(I386_REG_ECX, base_reg, offset);

					code_buf[code_idx++] = I386_INS_Grp5_Prefix;
					emit_base_offset(I386_INS_PUSH_Ev, I386_REG_ECX, 0);
				}
				else
				{
					code_buf[code_idx++] = I386_INS_Grp5_Prefix;
					emit_base_offset(I386_INS_PUSH_Ev, base_reg, offset);
				}
				break;
			case PUSH_ADDRESS:
				if (val_output)
				{
					if (opr->oprclass == TVAR_REF)
					{
						code_buf[code_idx++] = I386_INS_MOV_Gv_Ev;
						emit_base_offset(use_reg, base_reg, offset);
						code_buf[code_idx++] = I386_INS_Grp5_Prefix;
						emit_base_offset(I386_INS_PUSH_Ev, use_reg, offsetof(ht_ent_mname, value));
					}
					else
					{
						code_buf[code_idx++] = I386_INS_Grp5_Prefix;
						emit_base_offset(I386_INS_PUSH_Ev, base_reg, offset);
					}
				}
				else
				{
					code_buf[code_idx++] = I386_INS_LEA_Gv_M;
					emit_base_offset(I386_REG_ECX, base_reg, offset);

					code_buf[code_idx++] = I386_INS_PUSH_eCX;
				}
				break;
			case STORE:
				if (val_output)
				{
					if (use_reg == I386_REG_EAX)
						temp_reg = I386_REG_EDX;
					else
						temp_reg = I386_REG_EAX;
					assert(temp_reg != use_reg);
					code_buf[code_idx++] = I386_INS_MOV_Gv_Ev;
					emit_base_offset(temp_reg, base_reg, offset);
				}
				code_buf[code_idx++] = I386_INS_MOV_Ev_Gv;
				if (val_output)
					emit_base_offset(use_reg, temp_reg, 0);
				else
					emit_base_offset(use_reg, base_reg, offset);
				break;
			default:
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_UNIMPLOP);
				break;
			}
			break;
		default:
			GTMASSERT;
			break;
		}
		break;
	default:
		GTMASSERT;
		break;
	}
}


/*  Changes here, emit_base_offset, or emit_jmp may require changes in trip_gen case for
	OC_CALL[SP] and FORLCLDO
*/
void emit_xfer(short xfer)
{

	code_buf[code_idx++] = I386_INS_Grp5_Prefix;
	emit_base_offset(I386_INS_CALL_Ev, I386_REG_EBX, (int4)xfer);
}


void emit_op_base_offset(generic_op op, short base_reg, int offset, short use_reg)
{
	switch (op)
	{
	case CLEAR:
		code_buf[code_idx++] = I386_INS_MOV_Ev_Iv;
		emit_base_offset(0, base_reg, offset);
		*((int4 *)&code_buf[code_idx]) = 0;
		code_idx += SIZEOF(int4);
		break;
	case COMPARE:
		code_buf[code_idx++] = I386_INS_CMP_Gv_Ev;
		emit_base_offset(use_reg, base_reg, offset);
		break;
	case INCREMENT:
		code_buf[code_idx++] = I386_INS_Grp5_Prefix;
		emit_base_offset(I386_INS_INC_Ev, base_reg, offset);
		break;
	case LOAD:
		code_buf[code_idx++] = I386_INS_MOV_Gv_Ev;
		emit_base_offset(use_reg, base_reg, offset);
		break;
	case LOAD_ADDRESS:
		code_buf[code_idx++] = I386_INS_LEA_Gv_M;
		emit_base_offset(use_reg, base_reg, offset);
		break;
	case PUSH:
		code_buf[code_idx++] = I386_INS_Grp5_Prefix;
		emit_base_offset(I386_INS_PUSH_Ev, base_reg, offset);
		break;
	case PUSH_ADDRESS:
		code_buf[code_idx++] = I386_INS_LEA_Gv_M;
		emit_base_offset(use_reg, base_reg, offset);
		code_buf[code_idx++] = I386_INS_PUSH_eAX + use_reg;
		break;
	case STORE:
		code_buf[code_idx++] = I386_INS_MOV_Ev_Gv;
		emit_base_offset(use_reg, base_reg, offset);
		break;
	case TEST:
		code_buf[code_idx++] = I386_INS_Grp1_Ev_Ib_Prefix;
		emit_base_offset(I386_INS_CMP__, base_reg, offset);
		code_buf[code_idx++] = 0;
		break;
	default:
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_UNIMPLOP);
		break;
	}
}


/*  Changes here, emit_base_offset, or emit_jmp may require changes in trip_gen case for
	OC_CALL[SP] and FORLCLDO
*/
void emit_base_offset (short reg_opcode, short base_reg, int4 offset)
{
	modrm_byte.modrm.reg_opcode = reg_opcode;

	if (offset == 0)
		modrm_byte.modrm.mod = I386_MOD32_BASE;
	else if ((offset >= -128  &&  offset <= 127)  &&  force_32 == 0)
		modrm_byte.modrm.mod = I386_MOD32_BASE_DISP_8;
	else
		modrm_byte.modrm.mod = I386_MOD32_BASE_DISP_32;

	if (base_reg == I386_REG_ESP  ||  (base_reg == I386_REG_EBP  &&  offset == 0))
	{
		modrm_byte.modrm.r_m = I386_REG_SIB_FOLLOWS;
		code_buf[code_idx++] = modrm_byte.byte;

		sib_byte.sib.base = base_reg;
		sib_byte.sib.ss = I386_SS_TIMES_1;
		sib_byte.sib.index = I386_REG_NO_INDEX;
		code_buf[code_idx++] = sib_byte.byte;
	}
	else
	{
		modrm_byte.modrm.r_m = base_reg;
		code_buf[code_idx++] = modrm_byte.byte;
	}

	if (offset == 0)
		;
	else if ((offset >= -128  &&  offset <= 127)  &&  force_32 == 0)
		code_buf[code_idx++] = offset & 0xff;
	else
	{
		*((int4 *)&code_buf[code_idx]) = offset;
		code_idx += SIZEOF(int4);
	}
}


void emit_op_alit (generic_op op, unsigned char use_reg)
{
	switch (op)
	{
	case LOAD_ADDRESS:
		code_buf[code_idx++] = I386_INS_MOV_eAX + use_reg;
		break;
	case PUSH:
		code_buf[code_idx++] = I386_INS_Grp5_Prefix;
		modrm_byte.modrm.reg_opcode = I386_INS_PUSH_Ev;
		modrm_byte.modrm.mod = I386_MOD32_BASE;
		modrm_byte.modrm.r_m = I386_REG_disp32_NO_BASE;
		code_buf[code_idx++] = modrm_byte.byte;
		break;
	case PUSH_ADDRESS:
		code_buf[code_idx++] = I386_INS_PUSH_Iv;
		break;
	default:
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_UNIMPLOP);
		break;
	}
}


unsigned char i386_reg(unsigned char vax_reg)
{
	unsigned char	reg;

	switch (vax_reg & 0xf)	/* mask out VAX register mode field */
	{
	case 0:		reg = I386_REG_EAX;	break;
	case 1:		reg = I386_REG_EDX;	break;
	case 8:		reg = I386_REG_ESI;	break;
	case 9:		reg = I386_REG_EDI;	break;
	case 11:	reg = I386_REG_EBX;	break;
	default:
		GTMASSERT;
		break;
	}

	return reg;
}
