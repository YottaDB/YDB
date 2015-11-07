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

#include <sys/types.h>
#include "gtm_string.h"
#include <errno.h>
#include "gtm_fcntl.h"
#include "gtm_stat.h"
#include "gtm_stdio.h"

#include "compiler.h"
#include "opcode.h"
#include "mdq.h"
#include "vxi.h"
#include "vxt.h"
#include "cgp.h"
#include "obj_gen.h"
#include <rtnhdr.h>
#include "obj_file.h"
#include "list_file.h"
#include "min_max.h"
#include <emit_code.h>
#ifdef UNIX
#include "xfer_enum.h"
#endif
#include "hashtab_mname.h"
#include "stddef.h"

/* Required to find out variable length argument runtime function calls*/
#if defined(__x86_64__) || defined(__ia64)
#  include "code_address_type.h"
#  ifdef XFER
#    undef XFER
#  endif /* XFER */
#  define XFER(a,b) #b
#  include "xfer_desc.i"
DEFINE_XFER_TABLE_DESC;
GBLDEF int call_4lcldo_variant;	 /* used in emit_jmp for call[sp] and forlcldo */
#endif /* __x86_64__ || __ia64 */

#define MVAL_INT_SIZE DIVIDE_ROUND_UP(SIZEOF(mval), SIZEOF(UINTPTR_T))

#ifdef DEBUG
#  include "vdatsize.h"
/* VAX DISASSEMBLER TEXT */
static const char	vdat_bdisp[VDAT_BDISP_SIZE + 1] = "B^";
static const char	vdat_wdisp[VDAT_WDISP_SIZE + 1] = "W^";
static const char	vdat_r9[VDAT_R9_SIZE + 1] = "(r9)";
static const char	vdat_r8[VDAT_R8_SIZE + 1] = "(r8)";
static const char	vdat_gr[VDAT_GR_SIZE + 1] = "G^";
static const char	vdat_immed[VDAT_IMMED_SIZE + 1] = "I^#";
static const char	vdat_r11[VDAT_R11_SIZE + 1] = "(R11)";
static const char	vdat_gtmliteral[VDAT_GTMLITERAL_SIZE + 1] = "GTM$LITERAL";
static const char	vdat_def[VDAT_DEF_SIZE + 1] = "@";

IA64_ONLY(GBLDEF char	asm_mode = 0; /* 0 - disassembly mode. 1 - decode mode */)
GBLDEF unsigned char	*obpt;	  /* output buffer index */
GBLDEF unsigned char	outbuf[ASM_OUT_BUFF];	/* assembly language output buffer */
static int		vaxi_cnt = 1;	/* Vax instruction count */

/* Disassembler text: */
LITREF char		*xfer_name[];
LITREF char		vxi_opcode[][6];
GBLREF char 		*oc_tab_graphic[];
#endif

LITREF octabstruct	oc_tab[];	/* op-code table */
LITREF short		ttt[];		/* triple templates */

GBLREF boolean_t	run_time;
GBLREF int		sa_temps_offset[];
GBLREF int		sa_temps[];
LITREF int		sa_class_sizes[];

GBLDEF CODE_TYPE	code_buf[NUM_BUFFERRED_INSTRUCTIONS];
GBLDEF int		code_idx;
#ifdef DEBUG
GBLDEF struct inst_count generated_details[MAX_CODE_COUNT], calculated_details[MAX_CODE_COUNT];
GBLDEF int4 generated_count, calculated_count;
#endif /* DEBUG */
GBLDEF int		calculated_code_size, generated_code_size;
GBLDEF int		jmp_offset;	/* Offset to jump target */
GBLDEF int		code_reference;	/* Offset from pgm start to current loc */

DEBUG_ONLY(static boolean_t	opcode_emitted;)

static int		stack_depth = 0;

/* On x86_64, the smaller offsets are encoded in 1 byte (4 bytes otherwise). But for some cases,
  the offsets may be different during APPROX_ADDR and MACHINE phases, hence generating different size instruction.
  to solve this  even the smaller offsets need to be encoded in 4 bytes so that same size instructions are generated
  in both APPROX_ADDR and MACHINE phase. the variable  force_32 is used for this purpose*/
X86_64_ONLY(GBLDEF boolean_t force_32 = FALSE;)

GBLREF int		curr_addr;
GBLREF char		cg_phase;	/* code generation phase */
GBLREF char		cg_phase_last;	/* the previous code generation phase */


/*variables for counting the arguments*/
static int	vax_pushes_seen, vax_number_of_arguments;

static struct	push_list
{
	struct push_list	*next;
	unsigned char		value[PUSH_LIST_SIZE];
} *current_push_list_ptr, *push_list_start_ptr;

static int	push_list_index;
static boolean_t ocnt_ref_seen = FALSE;
static oprtype	*ocnt_ref_opr;
static triple	*current_triple;

error_def(ERR_MAXARGCNT);
error_def(ERR_SRCNAM);
error_def(ERR_UNIMPLOP);

void trip_gen (triple *ct)
{
	oprtype		**sopr, *opr;	/* triple operand */
	oprtype		*saved_opr[MAX_ARGS];
	unsigned short	oct;
	short		tp;		/* template pointer */
	const short	*tsp;		/* template short pointer */
	triple		*ttp;		/* temp triple pointer */
	int		irep_index;
	oprtype		*irep_opr;
	const short	*repl;		/* temp irep ptr */
	short		repcnt;
	int		off;

#	if !defined(TRUTH_IN_REG) && (!(defined(__osf__) || defined(__x86_64__) || defined(Linux390)))
	GTMASSERT;
#	endif
	DEBUG_ONLY(opcode_emitted = FALSE);
	current_triple = ct;	/* save for possible use by internal rtns */
	tp = ttt[ct->opcode];
	if (tp <= 0)
	{
		stx_error(ERR_UNIMPLOP);
		return;
	}
	code_idx = 0;
	vax_pushes_seen = 0;
	vax_number_of_arguments = 0;
	if (cg_phase_last != cg_phase)
	{
		cg_phase_last = cg_phase;
		if (cg_phase == CGP_APPROX_ADDR)
			push_list_init();
		else
			reset_push_list_ptr();
	}
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
	*sopr = 0;
	jmp_offset = 0;
	if (oct & OCT_JUMP || ct->opcode == OC_LDADDR || ct->opcode == OC_FORLOOP)
	{
		if (ct->operand[0].oprval.tref->rtaddr == 0)		/* forward reference */
		{
			jmp_offset = LONG_JUMP_OFFSET;
			assert(cg_phase == CGP_APPROX_ADDR);
		} else
			jmp_offset = ct->operand[0].oprval.tref->rtaddr - ct->rtaddr;

		switch(ct->opcode)
		{
			case OC_CALL:
			case OC_FORLCLDO:
			case OC_CALLSP:
#				ifdef __x86_64__
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
#				else
				off = (jmp_offset - CALL_INST_SIZE)/INST_SIZE;	/* [kmk] */
				if (off >= -128 && off <= 127)
					tsp = &ttt[ttt[tp]];
				else if (off >= -32768 && off <= 32767)
					tsp = &ttt[ttt[tp + 1]];
				else
					tsp = &ttt[ttt[tp + 2]];
				break;
#				endif /* __x86_64__ */
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
				tsp = &ttt[ttt[tp]];
				break;
			default:
				GTMASSERT;
		}
	} else if (oct & OCT_COERCE)
	{
		switch (oc_tab[ct->operand[0].oprval.tref->opcode].octype & (OCT_VALUE | OCT_BOOL))
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
		tsp = &ttt[tp];
	} else
		tsp = &ttt[tp];
	for (;  *tsp != VXT_END;)
	{
		if (*tsp == VXT_IREPAB || *tsp == VXT_IREPL)
		{
			repl = tsp;
			repl += 2;
			repcnt = *repl++;
			assert(repcnt != 1);
			for (irep_index = repcnt, irep_opr = &ct->operand[1];  irep_index > 2;  --irep_index)
			{
				assert(irep_opr->oprclass == TRIP_REF);
				irep_opr = &irep_opr->oprval.tref->operand[1];
			}
			if (irep_opr->oprclass == TRIP_REF)
			{
				repl = tsp;
				do
				{
					tsp = repl;
					tsp = emit_vax_inst((short *)tsp, &saved_opr[0], --sopr);
#					ifdef DEBUG
					if (cg_phase == CGP_ASSEMBLY)
						emit_asmlist(ct);
#					endif
				} while (sopr > &saved_opr[repcnt]);
			} else
			{
				sopr = &saved_opr[repcnt];
				tsp = repl;
			}
		} else
		{
			assert(*tsp > 0 && *tsp <= 511);
			tsp = emit_vax_inst((short *)tsp, &saved_opr[0], sopr);
#			ifdef DEBUG
			if (cg_phase == CGP_ASSEMBLY)
				emit_asmlist(ct);
#			endif
		} /* else */
	} /* for */
	if (cg_phase == CGP_APPROX_ADDR)
		if (vax_pushes_seen > 0)
			add_to_vax_push_list(vax_pushes_seen);
}

#ifdef DEBUG
void emit_asmlist(triple *ct)
{
	int		offset;
	unsigned char	*c;

	obpt -= 2;
	*obpt = ' ';	/* erase trailing comma */
	if (!opcode_emitted)
	{
		opcode_emitted = TRUE;
		offset = (int)(&outbuf[0] + 60 - obpt);
		if (offset >= 1)
		{	/* tab to position 60 */
			memset(obpt, ' ', offset);
			obpt += offset;
		} else
		{	/* leave at least 2 spaces */
			memset(obpt, ' ', 2);
			obpt += 2;
		}
		*obpt++ = ';';
		for (c = (unsigned char*)oc_tab_graphic[ct->opcode]; *c;)
			*obpt++ = *c++;
	}
	emit_eoi();
	format_machine_inst();
}

void	emit_eoi (void)
{
	IA64_ONLY(if (asm_mode == 0) {)
		*obpt++ = '\0';
		list_tab();
		list_line((char *)outbuf);
	IA64_ONLY(})
	return;
}
#endif


short	*emit_vax_inst (short *inst, oprtype **fst_opr, oprtype **lst_opr)
{
	static short	last_vax_inst = 0;
	short		sav_in, save_inst;
	boolean_t	oc_int;
	oprtype		*opr;
	triple		*ct;
	int		cnt, cnttop, reg, words_to_move, reg_offset, save_reg_offset, targ_reg;
	int		branch_idx, branch_offset, loop_top_idx, instr;

	code_idx = 0;
	switch (cg_phase)
	{
		case CGP_ASSEMBLY:
#			ifdef DEBUG
			list_chkpage();
			obpt = &outbuf[0];
			memset(obpt, SP, SIZEOF(outbuf));
			i2asc((uchar_ptr_t)obpt, vaxi_cnt++);
			obpt += 7;
			if (VXT_IREPAB != *inst && VXT_IREPL != *inst)
				instr = *inst;
			else
				instr = (*inst == VXT_IREPAB) ? VXI_PUSHAB : VXI_PUSHL;
			memcpy(obpt, &vxi_opcode[instr][0], 6);
			obpt += 10;
			*obpt++ = SP;
			*obpt++ = SP;
			/*****  WARNING - FALL THRU *****/
#			endif
		case CGP_ADDR_OPT:
		case CGP_APPROX_ADDR:
		case CGP_MACHINE:
			switch ((sav_in = *inst++))
			{
				case VXI_BEQL:
					emit_jmp(GENERIC_OPCODE_BEQ, &inst, GTM_REG_COND_CODE);
					break;
				case VXI_BGEQ:
					emit_jmp(GENERIC_OPCODE_BGE, &inst, GTM_REG_COND_CODE);
					break;
				case VXI_BGTR:
					emit_jmp(GENERIC_OPCODE_BGT, &inst, GTM_REG_COND_CODE);
					break;
				case VXI_BLEQ:
					emit_jmp(GENERIC_OPCODE_BLE, &inst, GTM_REG_COND_CODE);
					break;
				case VXI_BLSS:
					emit_jmp(GENERIC_OPCODE_BLT, &inst, GTM_REG_COND_CODE);
					break;
				case VXI_BNEQ:
					emit_jmp(GENERIC_OPCODE_BNE, &inst, GTM_REG_COND_CODE);
					break;
				case VXI_BLBC:
				case VXI_BLBS:
					assert(*inst == VXT_REG);
					inst++;
#					ifdef TRUTH_IN_REG
					reg = GTM_REG_CODEGEN_TEMP;
					NON_GTM64_ONLY(GEN_LOAD_WORD(reg, gtm_reg(*inst++), 0);)
					GTM64_ONLY(  GEN_LOAD_WORD_4(reg, gtm_reg(*inst++), 0);)
#					else
					/* For platforms, where the $TRUTH value is not carried in a register and
						must be fetched from a global variable by subroutine call. */
					assert(*inst == 0x5a);		/* VAX r10 or $TEST register */
					inst++;
					emit_call_xfer(SIZEOF(intszofptr_t) * xf_dt_get);
					reg = GTM_REG_R0;	/* function return value */
#					endif
					/* Generate a cmp instruction using the return value of the previous call,
						which will be in EAX */
					X86_64_ONLY(GEN_CMP_EAX_IMM32(0);)

					if (sav_in == VXI_BLBC)
						X86_64_ONLY(emit_jmp(GENERIC_OPCODE_BEQ, &inst, 0);)
						NON_X86_64_ONLY(emit_jmp(GENERIC_OPCODE_BLBC, &inst, reg);)
					else
					{
						assert(sav_in == VXI_BLBS);
						X86_64_ONLY(emit_jmp(GENERIC_OPCODE_BNE, &inst, 0);)
						NON_X86_64_ONLY(emit_jmp(GENERIC_OPCODE_BLBS, &inst, reg);)
					}
					break;
				case VXI_BRB:
					emit_jmp(GENERIC_OPCODE_BR, &inst, 0);
					break;
				case VXI_BRW:
					emit_jmp(GENERIC_OPCODE_BR, &inst, 0);
					break;
				case VXI_BICB2:
#					ifdef TRUTH_IN_REG
					GEN_CLEAR_TRUTH;
#					endif
					assert(*inst == VXT_LIT);
					inst++;
					assert(*inst == 1);
					inst++;
					assert(*inst == VXT_REG);
					inst++;
					inst++;
					break;
				case VXI_BISB2:
#					ifdef TRUTH_IN_REG
					GEN_SET_TRUTH;
#					endif
					assert(*inst == VXT_LIT);
					inst++;
					assert(*inst == 1);
					inst++;
					assert(*inst == VXT_REG);
					inst++;
					inst++;
					break;
				case VXI_CALLS:
					oc_int = TRUE;
					if (*inst == VXT_LIT)
					{
						inst++;
						cnt = (int4)*inst++;
					} else
					{
						assert(*inst == VXT_VAL);
						inst++;
						opr = *(fst_opr + *inst);
						assert(opr->oprclass == TRIP_REF);
						ct = opr->oprval.tref;
						if (ct->destination.oprclass)
							opr = &ct->destination;
#						ifdef __vms
						/* This is a case where VMS puts the argument count in a special register so
							handle that differently here.
						*/
						if (opr->oprclass == TRIP_REF)
						{
							assert(ct->opcode == OC_ILIT);
							cnt = ct->operand[0].oprval.ilit;
							code_buf[code_idx++] = ALPHA_INS_LDA
								| ALPHA_REG_AI << ALPHA_SHIFT_RA
								| ALPHA_REG_ZERO << ALPHA_SHIFT_RB
								| (cnt & ALPHA_MASK_DISP) << ALPHA_SHIFT_DISP;
							inst++;
						} else
						{
							assert(opr->oprclass == TINT_REF);
							oc_int = FALSE;
							opr = *(fst_opr + *inst++);
							reg = get_arg_reg();
							emit_trip(opr, TRUE, ALPHA_INS_LDL, reg);
							emit_push(reg);
						}
#						else
						/* All other platforms put argument counts in normal parameter
							registers and go through this path instead.
						*/
						if (opr->oprclass == TRIP_REF)
						{
							assert(ct->opcode == OC_ILIT);
							cnt = ct->operand[0].oprval.ilit;
							reg = get_arg_reg();
							IA64_ONLY(LOAD_IMM14(reg, cnt);)
							NON_IA64_ONLY(GEN_LOAD_IMMED(reg, cnt);)
							cnt++;
							inst++;
						} else
						{
							assert(opr->oprclass == TINT_REF);
							oc_int = FALSE;
							opr = *(fst_opr + *inst++);
							reg = get_arg_reg();
							emit_trip(opr, TRUE, GENERIC_OPCODE_LOAD, reg);
						}
						emit_push(reg);
#						endif
					}
					assert(*inst == VXT_XFER);
					inst++;
					emit_call_xfer((int)*inst++);
					if (oc_int)
					{
						if (cnt != 0)
							emit_pop(cnt);
					} else
					{	/* During the commonization of emit_code.c I discovered that TINT_REF is
							not currently used in the compiler so this may be dead code but I'm
							leaving this path in here anyway because I don't want to put it back
							in if we find we need it. (4/2003 SE)
						*/
						emit_trip(opr, TRUE, GENERIC_OPCODE_LOAD, CALLS_TINT_TEMP_REG);
						emit_pop(1);
					}
					break;
				case VXI_CLRL:
					assert(*inst == VXT_VAL);
					inst++;
					GEN_CLEAR_WORD_EMIT(CLRL_REG);
					break;
				case VXI_CMPL:
					assert(*inst == VXT_VAL);
					inst++;
					GEN_LOAD_WORD_EMIT(CMPL_TEMP_REG);
					assert(*inst == VXT_VAL);
					inst++;

					X86_64_ONLY(GEN_LOAD_WORD_EMIT(GTM_REG_CODEGEN_TEMP);)
					NON_X86_64_ONLY(GEN_LOAD_WORD_EMIT(GTM_REG_COND_CODE);)

					X86_64_ONLY(GEN_CMP_REGS(CMPL_TEMP_REG, GTM_REG_CODEGEN_TEMP))
					NON_X86_64_ONLY(GEN_SUBTRACT_REGS(CMPL_TEMP_REG, GTM_REG_COND_CODE, GTM_REG_COND_CODE);)
					break;
				case VXI_INCL:
					assert(*inst == VXT_VAL);
					inst++;
					save_inst = *inst++;
					emit_trip(*(fst_opr + save_inst), TRUE, GENERIC_OPCODE_LOAD, GTM_REG_ACCUM);
					GEN_ADD_IMMED(GTM_REG_ACCUM, 1);
					emit_trip(*(fst_opr + save_inst), TRUE, GENERIC_OPCODE_STORE, GTM_REG_ACCUM);
					break;
				case VXI_JMP:
					if (*inst == VXT_VAL)
					{
						inst++;
						emit_trip(*(fst_opr + *inst++), FALSE, GENERIC_OPCODE_LOAD, GTM_REG_CODEGEN_TEMP);
						GEN_JUMP_REG(GTM_REG_CODEGEN_TEMP);
					} else
						emit_jmp(GENERIC_OPCODE_BR, &inst, 0);
					break;
				case VXI_JSB:
					assert(*inst == VXT_XFER);
					inst++;
					emit_call_xfer((int)*inst++);
					/* Callee may have popped some values so we can't count on anything left on the stack. */
					stack_depth = 0;
					break;
				case VXI_MOVAB:
					if (*inst == VXT_JMP)
					{
						inst += 2;
						emit_pcrel();
						NON_RISC_ONLY(IGEN_LOAD_ADDR_REG(GTM_REG_ACCUM))
						RISC_ONLY(code_buf[code_idx++] |= IGEN_LOAD_ADDR_REG(GTM_REG_ACCUM);)
						assert(*inst == VXT_ADDR);
						inst++;
						emit_trip(*(fst_opr + *inst++), FALSE, GENERIC_OPCODE_STORE, GTM_REG_ACCUM);
					} else if (*inst == VXT_ADDR || *inst == VXT_VAL)
					{
						boolean_t	addr;

						addr = (*inst == VXT_VAL);
						inst++;
						save_inst = *inst++;
						assert(*inst == VXT_REG);
						inst++;
						emit_trip(*(fst_opr + save_inst), addr, GENERIC_OPCODE_LDA, gtm_reg(*inst++));
					} else
						GTMASSERT;
					break;
				case VXI_MOVC3:
					/* The MOVC3 instruction is only used to copy an mval from one place to another
						so that is the expansion we will generate. The most efficient expansion is to
						generate a series of load and store instructions. Do the loads first then the
						stores to keep the pipelines flowing and not stall waiting for any given load
						or store to complete. Because some platforms (notably HPPA) do not have enough
						argument registers to contain an entire MVAL and because an mval may grow from
						its present size and affect other platforms some day, We put the whole code gen
						thing in a loop so we can do this regardless of how big it gets.
					*/
					assert(*inst == VXT_LIT);
					inst += 2;
					assert(*inst == VXT_VAL);
					inst++;
					emit_trip(*(fst_opr + *inst++), TRUE, GENERIC_OPCODE_LDA, MOVC3_SRC_REG);
					assert(*inst == VXT_VAL);
					inst++;
					emit_trip(*(fst_opr + *inst++), TRUE, GENERIC_OPCODE_LDA, MOVC3_TRG_REG);
#					if defined(__MVS__) || defined(Linux390)
					/* The MVC instruction on zSeries facilitates memory copy(mval in this case) in a single
					 * instruction instead of multiple 8/4 byte copies.
					 *
					 * TODO: Revisit other platforms using generic emit_code and verify if the below
					 * logic of multiple copies can be replaced with more efficient instruction(s)
					 * available on that particular platform.
					 */
					GEN_MVAL_COPY(MOVC3_SRC_REG, MOVC3_TRG_REG, SIZEOF(mval));
#					else
					for (words_to_move = MVAL_INT_SIZE, reg_offset = 0; words_to_move;)
					{
						reg = MACHINE_FIRST_ARG_REG;
						save_reg_offset = reg_offset;
						for (cnt = 0, cnttop = MIN(words_to_move, MACHINE_REG_ARGS) ;  cnt < cnttop;
							cnt++, reg_offset += SIZEOF(UINTPTR_T))
						{
							X86_64_ONLY(targ_reg = GET_ARG_REG(cnt);)
							NON_X86_64_ONLY(targ_reg = reg + cnt;)
							NON_GTM64_ONLY(GEN_LOAD_WORD(targ_reg, MOVC3_SRC_REG, reg_offset);)
							GTM64_ONLY(GEN_LOAD_WORD_8(targ_reg, MOVC3_SRC_REG, reg_offset);)
						}
						reg = MACHINE_FIRST_ARG_REG;
						for (cnt = 0;
							cnt < cnttop;
							cnt++, save_reg_offset += SIZEOF(UINTPTR_T), words_to_move--)
						{
							X86_64_ONLY(targ_reg = GET_ARG_REG(cnt);)
							NON_X86_64_ONLY(targ_reg = reg + cnt;)
							NON_GTM64_ONLY(GEN_STORE_WORD(targ_reg, MOVC3_TRG_REG, save_reg_offset);)
							GTM64_ONLY(GEN_STORE_WORD_8(targ_reg, MOVC3_TRG_REG, save_reg_offset);)
						}
					}
#					endif
					break;
				case VXI_MOVL:
					if (*inst == VXT_REG)
					{
						inst++;
						if (*inst > 0x5f)	/* OC_CURRHD:  any mode >= 6 (deferred), any register */
						{
							inst++;
							NON_GTM64_ONLY(GEN_LOAD_WORD(GTM_REG_ACCUM, GTM_REG_FRAME_POINTER, 0);)
							GTM64_ONLY(GEN_LOAD_WORD_8(GTM_REG_ACCUM, GTM_REG_FRAME_POINTER, 0);)
							assert(*inst == VXT_ADDR);
							inst++;
							emit_trip(*(fst_opr + *inst++), FALSE, GENERIC_OPCODE_STORE, GTM_REG_ACCUM);
						} else
						{
							boolean_t addr;

							assert(*inst == 0x50);  /* register mode: (from) r0 */
							inst++;
							if (*inst == VXT_VAL || *inst == VXT_ADDR)
							{
								addr = (*inst == VXT_VAL);
								inst++;
								emit_trip(*(fst_opr + *inst++), addr, GENERIC_OPCODE_STORE,
									  MOVL_RETVAL_REG);
							} else if (*inst == VXT_REG)
							{
								inst++;

#								ifdef TRUTH_IN_REG
								if (*inst == 0x5a)	/* to VAX r10 or $TEST */
								{
									NON_GTM64_ONLY(GEN_STORE_WORD(MOVL_RETVAL_REG,
													GTM_REG_DOLLAR_TRUTH, 0);)
									GTM64_ONLY(GEN_STORE_WORD_4(MOVL_RETVAL_REG,
													GTM_REG_DOLLAR_TRUTH, 0);)
								} else
								{
									GEN_MOVE_REG(gtm_reg(*inst), MOVL_RETVAL_REG);
								}
#								else
								if (*inst == 0x5a)	/* to VAX r10 or $TEST */
								{
									reg = get_arg_reg();
									GEN_MOVE_REG(reg, MOVL_RETVAL_REG);
									emit_push(reg);
									emit_call_xfer(SIZEOF(intszofptr_t) * xf_dt_store);
								} else
								{
									GEN_MOVE_REG(gtm_reg(*inst), MOVL_RETVAL_REG);
								}
#								endif
								inst++;
							} else
								GTMASSERT;
						}
					} else if (*inst == VXT_VAL)
					{
						inst++;
						save_inst = *inst++;
						assert(*inst == VXT_REG);
						inst++;
						assert(*inst == 0x51);  /* register mode: R1 */
						inst++;
						emit_trip(*(fst_opr + save_inst), TRUE, GENERIC_OPCODE_LOAD, MOVL_REG_R1);
					} else
						GTMASSERT;
					break;
				case VXT_IREPAB:
					assert(*inst == VXT_VAL);
					inst += 2;
					reg = get_arg_reg();
					emit_trip(*lst_opr, TRUE, GENERIC_OPCODE_LDA, reg);
					emit_push(reg);
					break;
				case VXI_PUSHAB:
					reg = get_arg_reg();
					if (*inst == VXT_JMP)
					{
						inst += 2;
						emit_pcrel();
						NON_RISC_ONLY(IGEN_LOAD_ADDR_REG(reg))
						RISC_ONLY(code_buf[code_idx++] |= IGEN_LOAD_ADDR_REG(reg);)
					} else if (*inst == VXT_VAL  ||  *inst == VXT_GREF)
					{
						inst++;
						emit_trip(*(fst_opr + *inst++), TRUE, GENERIC_OPCODE_LDA, reg);
					} else
						GTMASSERT;
					emit_push(reg);
					break;
				case VXT_IREPL:
					assert(*inst == VXT_VAL);
					inst += 2;
					reg = get_arg_reg();
					emit_trip(*lst_opr, TRUE, GENERIC_OPCODE_LOAD, reg);
					emit_push(reg);
					break;
				case VXI_PUSHL:
					reg = get_arg_reg();
					if (*inst == VXT_LIT)
					{
						inst++;
						GEN_LOAD_IMMED(reg, *inst);
						inst++;
					} else if (*inst == VXT_ADDR)
					{
						inst++;
						emit_trip(*(fst_opr + *inst++), FALSE, GENERIC_OPCODE_LOAD, reg);
					} else if (*inst == VXT_VAL)
					{
						inst++;
						emit_trip(*(fst_opr + *inst++), TRUE, GENERIC_OPCODE_LOAD, reg);
					} else
						GTMASSERT;
					emit_push(reg);
					break;
				case VXI_TSTL:
					assert(*inst == VXT_VAL || *inst == VXT_REG);
					if (*inst == VXT_VAL)
					{
						inst++;
						emit_trip(
							*(fst_opr + *inst++), TRUE, GENERIC_OPCODE_LOAD,
							X86_64_ONLY(GTM_REG_CODEGEN_TEMP) NON_X86_64_ONLY(GTM_REG_COND_CODE)
						);
						X86_64_ONLY(GEN_CMP_IMM32(GTM_REG_CODEGEN_TEMP, 0))
					} else if (*inst == VXT_REG)
					{
						inst++;
						X86_64_ONLY(assert(gtm_reg(*inst) == I386_REG_RAX); /* Same as R0 */)
						X86_64_ONLY(GEN_CMP_EAX_IMM32(0);)
						NON_X86_64_ONLY(GEN_MOVE_REG(GTM_REG_COND_CODE, gtm_reg(*inst));)
						inst++;
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
	assert(code_idx < NUM_BUFFERRED_INSTRUCTIONS);
	if (cg_phase == CGP_MACHINE)
	{
		generated_code_size += code_idx;
#		ifdef DEBUG
		if (generated_count < MAX_CODE_COUNT)
		{
			generated_details[generated_count].size = code_idx;
			generated_details[generated_count++].sav_in = sav_in;
		}
#		endif /* DEBUG */
		emit_immed ((char *)&code_buf[0], (uint4)(INST_SIZE * code_idx));
	} else if (cg_phase != CGP_ASSEMBLY)
	{
		if (cg_phase == CGP_APPROX_ADDR)
		{
			calculated_code_size += code_idx;
#			ifdef DEBUG
			if (calculated_count < MAX_CODE_COUNT)
			{
				calculated_details[calculated_count].size = code_idx;
				calculated_details[calculated_count++].sav_in = sav_in;
			}
#			endif /* DEBUG */
		}
		curr_addr += (INST_SIZE * code_idx);
	}
	code_reference += (INST_SIZE * code_idx);
	jmp_offset -= (INST_SIZE * code_idx);
	last_vax_inst = sav_in;
	return inst;
}

#ifndef __x86_64__ /* For x86_64, this is defined in emit_code_sp.c */
void	emit_jmp (uint4 branchop, short **instp, int reg)
{
	uint4 	branchop_opposite;
	int	src_reg;
	int	skip_idx;
	NON_RISC_ONLY(int tmp_code_idx;)
	int	branch_offset;

	/* assert(jmp_offset != 0); */
	/* assert commented since jmp_offset could be zero in CGP_ADDR_OPT phase after a jump to the immediately following
	 * instruction is nullified (as described below) */

	/* size of this particular instruction */
	jmp_offset -= (int)((char *)&code_buf[code_idx] - (char *)&code_buf[0]);
#	if !(defined(__MVS__) || defined(Linux390))
	/* The code_buff on zSeries is filled with 2 byte chunks */
	assert((jmp_offset & 3) == 0);
#	endif
	branch_offset = jmp_offset / INST_SIZE;
	/* Some platforms have a different origin for the offset */
	EMIT_JMP_ADJUST_BRANCH_OFFSET;
	switch (cg_phase)
	{
#		ifdef DEBUG
		case CGP_ASSEMBLY:
			*obpt++ = 'x';
			*obpt++ = '^';
			*obpt++ = '0';
			*obpt++ = 'x';
			obpt += i2hex_nofill(INST_SIZE * branch_offset, (uchar_ptr_t)obpt, 8);
			*obpt++ = ',';
			*obpt++ = ' ';
		/*****  WARNING - FALL THRU *****/
#		endif
		case CGP_ADDR_OPT:
		case CGP_APPROX_ADDR:
		case CGP_MACHINE:
			assert(VXT_JMP == **instp);
			*instp += 1;
			assert(1 == **instp);
			(*instp)++;
			if (0 == branch_offset)
			{	/* This is a jump to the immediately following instruction. Nullify the jump
					and don't generate any instruction (not even a NOP) */
				/* code_buf[code_idx++] = GENERIC_OPCODE_NOP; */
			} else if (EMIT_JMP_SHORT_CODE_CHECK)
			{	/* Short jump immediate operand - some platforms also do a compare */
				EMIT_JMP_SHORT_CODE_GEN;
			} else
			{	/* Potentially longer jump sequence */
				skip_idx = -1;
				if (EMIT_JMP_OPPOSITE_BR_CHECK)
				{	/* This jump sequence is longer and is not conditional so if we need a conditional
						jump, create the opposite conditional jump to jump around the longer jump to
						the target thereby preserving the original semantics.
					*/
					EMIT_JMP_GEN_COMPARE;
					switch (branchop)
					{
						case GENERIC_OPCODE_BEQ:
							branchop_opposite = GENERIC_OPCODE_BNE;
							break;
						case GENERIC_OPCODE_BGE:
							branchop_opposite = GENERIC_OPCODE_BLT;
							break;
						case GENERIC_OPCODE_BGT:
							branchop_opposite = GENERIC_OPCODE_BLE;
							break;
						case GENERIC_OPCODE_BLE:
							branchop_opposite = GENERIC_OPCODE_BGT;
							break;
						case GENERIC_OPCODE_BLT:
							branchop_opposite = GENERIC_OPCODE_BGE;
							break;
						case GENERIC_OPCODE_BNE:
							branchop_opposite = GENERIC_OPCODE_BEQ;
							break;
#						ifdef __alpha
						case GENERIC_OPCODE_BLBC:
							branchop_opposite = GENERIC_OPCODE_BLBS;
							break;
						case GENERIC_OPCODE_BLBS:
							branchop_opposite = GENERIC_OPCODE_BLBC;
							break;
#						endif
						default:
							GTMASSERT;
							break;
					}
					RISC_ONLY(
						skip_idx = code_idx++; /* Save index of branch inst. Set target offset later */
						code_buf[skip_idx] = IGEN_COND_BRANCH_REG_OFFSET(branchop_opposite, reg, 0);
						branch_offset--;
					)
					NON_RISC_ONLY(
						skip_idx = code_idx; /* Save index of branch inst. Set target offset later */
						IGEN_COND_BRANCH_REG_OFFSET(branchop_opposite, reg, 0)
						branch_offset -= NUM_INST_IGEN_COND_BRANCH_REG_OFFSET;
					)
#					ifdef DELAYED_BRANCH
						code_buf[code_idx++] = GENERIC_OPCODE_NOP;
						branch_offset--;
#					endif
				}
				if (EMIT_JMP_LONG_CODE_CHECK)
				{ /* This is more common unconditional branch generation and should be mutually
					exclusive to EMIT_JMP_OPPOSITE_BR_CHECK. Some platforms will have the "short"
					branch generation up top be more common but that form does not cover unconditional
					jumps (Examples: AIX and HP-UX) */
					assert(!(EMIT_JMP_OPPOSITE_BR_CHECK));
					NON_RISC_ONLY(IGEN_UCOND_BRANCH_REG_OFFSET(branchop, branch_offset))
					RISC_ONLY(
					code_buf[code_idx++] = IGEN_UCOND_BRANCH_REG_OFFSET(branchop, 0, branch_offset);
					)
#					ifdef DELAYED_BRANCH
					code_buf[code_idx++] = GENERIC_OPCODE_NOP;
#					endif
				} else
				{
					if (EMIT_JMP_OPPOSITE_BR_CHECK)
					{  /* VAX conditional long jump generates two native branch instructions -
						one conditional branch (above) and one PC relative branch (below).
						The second branch instruction also needs adjustment of the origin. */
						EMIT_JMP_ADJUST_BRANCH_OFFSET;
					}
					GEN_PCREL;
					emit_base_offset(GTM_REG_CODEGEN_TEMP, (INST_SIZE * branch_offset));
					RISC_ONLY(code_buf[code_idx++] |= IGEN_LOAD_ADDR_REG(GTM_REG_CODEGEN_TEMP);)
					NON_RISC_ONLY(IGEN_LOAD_ADDR_REG(GTM_REG_CODEGEN_TEMP))
					GEN_JUMP_REG(GTM_REG_CODEGEN_TEMP);
				}
				if (skip_idx != -1)
				{	/* Fill in the offset from our opposite jump instruction to here .. the
						place to bypass the jump.
					*/
					branch_offset = BRANCH_OFFSET_FROM_IDX(skip_idx, code_idx);
					RISC_ONLY(code_buf[skip_idx] |= IGEN_COND_BRANCH_OFFSET(branch_offset);)

					NON_RISC_ONLY(
						tmp_code_idx = code_idx;
						code_idx = skip_idx;
						IGEN_COND_BRANCH_REG_OFFSET(branchop_opposite, reg, branch_offset)
						code_idx = tmp_code_idx;
					)
				}
			}
			break;
		default:
			GTMASSERT;
			break;
	}
}

#endif /* !__x86_64__ */

/* Emit code that generates a relative pc based jump target. The last instruction is not
	complete so the caller may finish it with whatever instruction is necessary.
*/
void	emit_pcrel(void)
{
	int branch_offset;

	jmp_offset -= INTCAST((char *)&code_buf[code_idx] - (char *)&code_buf[0]);
	switch (cg_phase)
	{
#		ifdef DEBUG
		case CGP_ASSEMBLY:
			*obpt++ = 'x';
			*obpt++ = '^';
			*obpt++ = '0';
			*obpt++ = 'x';
			obpt += i2hex_nofill(jmp_offset + code_reference, (uchar_ptr_t)obpt, 8);
			*obpt++ = ',';
			*obpt++ = ' ';
				/*****  WARNING - FALL THRU *****/
#		endif
		case CGP_ADDR_OPT:
		case CGP_APPROX_ADDR:
		case CGP_MACHINE:
			branch_offset = jmp_offset / INST_SIZE;
			GEN_PCREL;
			EMIT_JMP_ADJUST_BRANCH_OFFSET;  /* Account for different branch origins on different platforms */
			emit_base_offset(GTM_REG_CODEGEN_TEMP, INST_SIZE * branch_offset);
			break;
		default:
			GTMASSERT;
			break;
	}
}


/* Emit the code for a given triple */
void	emit_trip(oprtype *opr, boolean_t val_output, uint4 generic_inst, int trg_reg)
{
	boolean_t	inst_emitted;
	unsigned char	reg, op_mod, op_reg;
	int		offset, immediate;
	int		upper_idx, lower_idx;
	triple		*ct;
	int		low, extra, high;
	GTM64_ONLY(int	next_ptr_offset = 8;)
	if (opr->oprclass == TRIP_REF)
	{
		ct = opr->oprval.tref;
		if (ct->destination.oprclass)
			opr = &ct->destination;
		/* else lit or error */
	}
	inst_emitted = FALSE;
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
							assert(ct->operand[0].oprclass == MLIT_REF);
							if (run_time)
								reg = GTM_REG_PV;
							else
								reg = GTM_REG_LITERAL_BASE;

							if (CGP_ADDR_OPT == cg_phase)
							{
							/* We want the expansion to be proper sized this time. Note
								that this won't be true so much on the initial CGP_ADDR_OPT
								pass but will be true on the shrink_trips() pass after the
								literals are compiled.
							*/
								offset = literal_offset(ct->operand[0].oprval.mlit->rt_addr);
								/* Need non-zero base reg for AIX */
								X86_64_ONLY(force_32 = TRUE;)
								emit_base_offset(reg, offset);
								X86_64_ONLY(force_32 = FALSE;)
							} else
							{
								/* Gross expansion ok first time through */
								/* Non-0 base reg for AIX */
								X86_64_ONLY(force_32 = TRUE;)
								emit_base_offset(reg, LONG_JUMP_OFFSET);
								X86_64_ONLY(force_32 = FALSE;)
							}

							X86_64_ONLY(IGEN_LOAD_ADDR_REG(trg_reg))
#							if !(defined(__MVS__) || defined(Linux390))
							NON_X86_64_ONLY(code_idx++;)
#							else
							IGEN_LOAD_ADDR_REG(trg_reg);
#							endif
							inst_emitted = TRUE;
							break;
						case OC_CDLIT:
							emit_base_offset(GTM_REG_PV, find_linkage(ct->operand[0].oprval.cdlt));
							if (GENERIC_OPCODE_LDA == generic_inst)
							{
								RISC_ONLY(code_buf[code_idx++] |= IGEN_LOAD_LINKAGE(trg_reg);)
								NON_RISC_ONLY(IGEN_LOAD_LINKAGE(trg_reg);)
								inst_emitted = TRUE;
							} else
							{
								RISC_ONLY(code_buf[code_idx++]
									  |= IGEN_LOAD_LINKAGE(GTM_REG_CODEGEN_TEMP);)
								NON_RISC_ONLY(IGEN_LOAD_LINKAGE(GTM_REG_CODEGEN_TEMP);)
								emit_base_offset(GTM_REG_CODEGEN_TEMP, 0);
							}
							break;
						case OC_ILIT:
							assert(GENERIC_OPCODE_LOAD == generic_inst);
							immediate = ct->operand[0].oprval.ilit;
							EMIT_TRIP_ILIT_GEN;
							inst_emitted = TRUE;
							break;
						case OC_TRIPSIZE:
							/* This tiples value is calculated in the shrink_jmp/shrink_trips
								phase. It is a parameter to (currently only) op_exfun and is the
								length of the generated jump instruction. op_exfun needs this
								length to adjust the return address in the created stackframe
								so it does not have to parse instructions at the return address
								to see what return signature was created. We will add asserts to
								this generation in later phases after the true value has been
								calculated. At this point, it is zero.
							*/
							immediate = ct->operand[0].oprval.tsize->size;
							EMIT_TRIP_ILIT_GEN;
							inst_emitted = TRUE;
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
					GTM64_ONLY(
						if ( sa_class_sizes[opr->oprclass] == 4 )
						{
							next_ptr_offset = 4;
							REVERT_GENERICINST_TO_WORD(generic_inst);
						}
					)
					NON_GTM64_ONLY(
						if (offset < 0  ||  offset > MAX_OFFSET)
							GTMASSERT;
					)
					emit_base_offset(GTM_REG_FRAME_TMP_PTR, offset);
					break;

				case TCAD_REF:
				case TVAD_REF:
				case TVAR_REF:
					offset = sa_temps_offset[opr->oprclass];
					offset -= (sa_temps[opr->oprclass] - opr->oprval.temp) * sa_class_sizes[opr->oprclass];
					GTM64_ONLY(
						if ( sa_class_sizes[opr->oprclass] == 4 )
						{
							next_ptr_offset = 4;
							REVERT_GENERICINST_TO_WORD(generic_inst);
						}
					)
					NON_GTM64_ONLY(
						if (offset < 0  ||  offset > MAX_OFFSET)
							GTMASSERT;
					)
					if (opr->oprclass == TVAR_REF)
						reg = GTM_REG_FRAME_VAR_PTR;
					else
						reg = GTM_REG_FRAME_TMP_PTR;
					emit_base_offset(reg, offset);
					if (val_output)
					{
						if (GENERIC_OPCODE_LDA == generic_inst)
						{
							NON_RISC_ONLY(IGEN_LOAD_NATIVE_REG(trg_reg))
							RISC_ONLY(code_buf[code_idx++] |= IGEN_LOAD_NATIVE_REG(trg_reg);)
							if (opr->oprclass == TVAR_REF)
							{
								emit_base_offset(trg_reg, offsetof(ht_ent_mname, value));
								NON_RISC_ONLY(IGEN_LOAD_NATIVE_REG(trg_reg))
								RISC_ONLY(code_buf[code_idx++] |= IGEN_LOAD_NATIVE_REG(trg_reg);)
							}
							inst_emitted = TRUE;
						} else
						{
							NON_RISC_ONLY(IGEN_LOAD_NATIVE_REG(GTM_REG_CODEGEN_TEMP))
							RISC_ONLY(code_buf[code_idx++]
									|= IGEN_LOAD_NATIVE_REG(GTM_REG_CODEGEN_TEMP);)
							emit_base_offset(GTM_REG_CODEGEN_TEMP, 0);
						}
					}
					break;
				case OCNT_REF:
					/* This ref's value is calculated in emit_call_xfer(). This value is related to TSIZ_REF
					   in that it is used for the same reason to different calls (op_call, op_callsp,
					   op_forlcldo, and their mprof counterparts). It is the offset needed to be
					   added to the return address from the calls to these routines to bypass a
					   generated jump sequence. In this case however, the jump sequence is being
					   generated as part of the OC_CALL, OC_CALLSP or OC_FORLCLDO triple itself.
					   There is no separate jump triple so the TSIZ_REF triple cannot be used.
					   So this operand is the OFFSET from the CALL to the NEXT TRIPLE. The operation
					   is that when this routine sees this type of reference, it will set a flag
					   and record the operand address and go ahead and generate the value that it has. The
					   next transfer table generation that occurs will see the set flag and will compute
					   the address from the return address of that transfer table call to the next triple
					   and update this triple's value. Since our originating triple has a JUMP type,
					   it will be updated in shrink_jmp/shirnk_trips() until all necessary shrinkage
					   is done so the final phase will have the correct value and we only have to
					   generate an immediate value.
					*/
					immediate = opr->oprval.offset;
					EMIT_TRIP_ILIT_GEN;
					inst_emitted = TRUE;
					ocnt_ref_seen = TRUE;
					ocnt_ref_opr = opr;
					break;
			}
			if (!inst_emitted) {
				NON_RISC_ONLY(IGEN_GENERIC_REG(generic_inst, trg_reg))
				RISC_ONLY(code_buf[code_idx++] |= IGEN_GENERIC_REG(generic_inst, trg_reg);)
			}
			break;
#		ifdef DEBUG
		case CGP_ASSEMBLY:
			offset = 0;
			switch (opr->oprclass)
			{
				case TRIP_REF:
					assert(ct->destination.oprclass == 0);
					assert(val_output);
					switch (ct->opcode)
					{
						case OC_LIT:
							assert(ct->operand[0].oprclass == MLIT_REF);
							offset = literal_offset(ct->operand[0].oprval.mlit->rt_addr);
							memcpy(obpt, &vdat_def[0], VDAT_DEF_SIZE);
							obpt += VDAT_DEF_SIZE;
							memcpy(obpt, &vdat_gtmliteral[0], VDAT_GTMLITERAL_SIZE);
							obpt += VDAT_GTMLITERAL_SIZE;
							*obpt++ = '+';
							*obpt++ = '0';
							*obpt++ = 'x';
							obpt += i2hex_nofill(offset, (uchar_ptr_t)obpt, 8);
							if (run_time)
								reg = GTM_REG_PV;
							else
								reg = GTM_REG_LITERAL_BASE;
							X86_64_ONLY(force_32 = TRUE;)
							emit_base_offset(reg, offset);
							X86_64_ONLY(force_32 = FALSE;)
							NON_RISC_ONLY(IGEN_LOAD_ADDR_REG(trg_reg))
							RISC_ONLY(code_buf[code_idx++] |= IGEN_LOAD_ADDR_REG(trg_reg);)
							inst_emitted = TRUE;
							break;
						case OC_CDLIT:
							if (val_output)
							{
								memcpy(obpt, &vdat_gr[0], VDAT_GR_SIZE);
								obpt += VDAT_GR_SIZE;
							}
							memcpy(obpt, ct->operand[0].oprval.cdlt->addr,
								ct->operand[0].oprval.cdlt->len);
							obpt += ct->operand[0].oprval.cdlt->len;
							emit_base_offset(GTM_REG_PV, find_linkage(ct->operand[0].oprval.cdlt));
							if (GENERIC_OPCODE_LDA == generic_inst)
							{
								RISC_ONLY(code_buf[code_idx++] |= IGEN_LOAD_LINKAGE(trg_reg));
								NON_RISC_ONLY(IGEN_LOAD_LINKAGE(trg_reg));
								inst_emitted = TRUE;
							} else
							{
								RISC_ONLY(code_buf[code_idx++]
										|= IGEN_LOAD_LINKAGE(GTM_REG_CODEGEN_TEMP);)
								NON_RISC_ONLY(IGEN_LOAD_LINKAGE(GTM_REG_CODEGEN_TEMP);)
								emit_base_offset(GTM_REG_CODEGEN_TEMP, 0);
							}
							break;
						case OC_ILIT:
							assert(generic_inst == GENERIC_OPCODE_LOAD);
							immediate = ct->operand[0].oprval.ilit;
							memcpy(obpt, &vdat_immed[0], VDAT_IMMED_SIZE);
							obpt += VDAT_IMMED_SIZE;
							obpt = i2asc((uchar_ptr_t)obpt, ct->operand[0].oprval.ilit);
							EMIT_TRIP_ILIT_GEN;
							inst_emitted = TRUE;
							break;
						case OC_TRIPSIZE:
							immediate = ct->operand[0].oprval.tsize->size;
							assert(0 < immediate);
							assert(MAX_BRANCH_CODEGEN_SIZE > immediate);
							EMIT_TRIP_ILIT_GEN;
							inst_emitted = TRUE;
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
					NON_GTM64_ONLY(
						if (offset < 0  ||  offset > MAX_OFFSET)
							GTMASSERT;
					)
					if (offset < 127)
					{
						memcpy(obpt, &vdat_bdisp[0], VDAT_BDISP_SIZE);
						obpt += VDAT_BDISP_SIZE;
					} else
					{
						memcpy(obpt, &vdat_wdisp[0], VDAT_WDISP_SIZE);
						obpt += VDAT_WDISP_SIZE;
					}
					obpt = i2asc((uchar_ptr_t)obpt, offset);
					memcpy(obpt, &vdat_r9[0], VDAT_R9_SIZE);
					obpt += VDAT_R9_SIZE;
					/*
					 * for 64 bit platforms, By default the loads/stores
					 * are of 8 bytes, but if the value being dealt with
					 * is a word, then the opcode in generic_inst is
					 * changed to ldw/stw(4 byte load/stores)
					 */
					GTM64_ONLY(
						if (sa_class_sizes[opr->oprclass] == 4)
						{
							next_ptr_offset = 4;
							REVERT_GENERICINST_TO_WORD(generic_inst);
						}
					)
					emit_base_offset(GTM_REG_FRAME_TMP_PTR, offset);
					break;
				case TCAD_REF:
				case TVAD_REF:
				case TVAR_REF:
					offset = sa_temps_offset[opr->oprclass];
					offset -= (sa_temps[opr->oprclass] - opr->oprval.temp) * sa_class_sizes[opr->oprclass];
					if (val_output)
					{
						memcpy(obpt, &vdat_def[0], VDAT_DEF_SIZE);
						obpt += VDAT_DEF_SIZE;
					}
					if (offset < 127)
					{
						memcpy(obpt, &vdat_bdisp[0], VDAT_BDISP_SIZE);
						obpt += VDAT_BDISP_SIZE;
					} else
					{
						memcpy(obpt, &vdat_wdisp[0], VDAT_WDISP_SIZE);
						obpt += VDAT_WDISP_SIZE;
					}
					obpt = i2asc((uchar_ptr_t)obpt, offset);
					if (opr->oprclass == TVAR_REF)
					{
						memcpy(obpt, &vdat_r8[0], VDAT_R8_SIZE);
						obpt += VDAT_R8_SIZE;
					} else
					{
						memcpy(obpt, &vdat_r9[0], VDAT_R9_SIZE);
						obpt += VDAT_R9_SIZE;
					}
					NON_GTM64_ONLY(
						if (offset < 0  ||  offset > MAX_OFFSET)
							GTMASSERT;
					)
					if (opr->oprclass == TVAR_REF)
						reg = GTM_REG_FRAME_VAR_PTR;
					else
						reg = GTM_REG_FRAME_TMP_PTR;
					GTM64_ONLY(
						if (sa_class_sizes[opr->oprclass] == 4)
						{
							next_ptr_offset = 4;
							REVERT_GENERICINST_TO_WORD(generic_inst);
						}
					)
					emit_base_offset(reg, offset);
					if (val_output)	/* indirection */
					{

						if (GENERIC_OPCODE_LDA == generic_inst)
						{
							RISC_ONLY(code_buf[code_idx++] |= IGEN_LOAD_NATIVE_REG(trg_reg);)
							NON_RISC_ONLY(IGEN_LOAD_NATIVE_REG(trg_reg))
							if (opr->oprclass == TVAR_REF)
							{
								emit_base_offset(trg_reg, offsetof(ht_ent_mname, value));
								NON_RISC_ONLY(IGEN_LOAD_NATIVE_REG(trg_reg))
								RISC_ONLY(code_buf[code_idx++] |= IGEN_LOAD_NATIVE_REG(trg_reg);)
							}
							inst_emitted = TRUE;
						} else
						{
							RISC_ONLY(code_buf[code_idx++]
									|= IGEN_LOAD_NATIVE_REG(GTM_REG_CODEGEN_TEMP);)
							NON_RISC_ONLY(IGEN_LOAD_NATIVE_REG(GTM_REG_CODEGEN_TEMP))
							emit_base_offset(GTM_REG_CODEGEN_TEMP, 0);
						}
					}
					break;
				case OCNT_REF:
					immediate = opr->oprval.offset;
					assert(0 < immediate);
					assert(MAX_BRANCH_CODEGEN_SIZE > immediate);
					EMIT_TRIP_ILIT_GEN;
					inst_emitted = TRUE;
					ocnt_ref_seen = TRUE;
					ocnt_ref_opr = opr;
					break;

				default:
					GTMASSERT;
					break;
			}
			if (!inst_emitted) {
				RISC_ONLY(code_buf[code_idx++] |= IGEN_GENERIC_REG(generic_inst, trg_reg);)
				NON_RISC_ONLY(IGEN_GENERIC_REG(generic_inst, trg_reg))
			}
			*obpt++ = ',';
			*obpt++ = ' ';
			break;
#		endif
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
							offset = literal_offset(ct->operand[0].oprval.mlit->rt_addr);
							if (run_time)
								reg = GTM_REG_PV;
							else
								reg = GTM_REG_LITERAL_BASE;
							X86_64_ONLY(force_32 = TRUE;)
							emit_base_offset(reg, offset);
							X86_64_ONLY(force_32 = FALSE;)
							RISC_ONLY(code_buf[code_idx++] |= IGEN_LOAD_ADDR_REG(trg_reg);)
							NON_RISC_ONLY(IGEN_LOAD_ADDR_REG(trg_reg);)
							inst_emitted = TRUE;
							break;
						case OC_CDLIT:
							emit_base_offset(GTM_REG_PV, find_linkage(ct->operand[0].oprval.cdlt));
							if (GENERIC_OPCODE_LDA == generic_inst)
							{
								RISC_ONLY(code_buf[code_idx++] |= IGEN_LOAD_LINKAGE(trg_reg);)
								NON_RISC_ONLY(IGEN_LOAD_LINKAGE(trg_reg);)
								inst_emitted = TRUE;
							} else
							{
	 							RISC_ONLY(code_buf[code_idx++]
										|= IGEN_LOAD_LINKAGE(GTM_REG_CODEGEN_TEMP);)
								NON_RISC_ONLY(IGEN_LOAD_LINKAGE(GTM_REG_CODEGEN_TEMP);)
								emit_base_offset(GTM_REG_CODEGEN_TEMP, 0);
							}
							break;
						case OC_ILIT:
							assert(GENERIC_OPCODE_LOAD == generic_inst);
							immediate = ct->operand[0].oprval.ilit;
							EMIT_TRIP_ILIT_GEN;
							inst_emitted = TRUE;
							break;
						case OC_TRIPSIZE:
							immediate = ct->operand[0].oprval.tsize->size;
#							if !defined(__osf__) && !defined(__hppa)
							/* Legitimate but odd call to next M line on Tru64 and HPUX-HPPA gives an
							 * immediate value of zero because of how offsets are calculated on
							 * these platforms so bypass the assert for them.
							 */
							assert(0 < immediate);
#							endif
							assert(MAX_BRANCH_CODEGEN_SIZE > immediate);
							EMIT_TRIP_ILIT_GEN;
							inst_emitted = TRUE;
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
					GTM64_ONLY(
						if ( sa_class_sizes[opr->oprclass] == 4 )
						{
							next_ptr_offset = 4;
							REVERT_GENERICINST_TO_WORD(generic_inst);
						}
					)

					NON_GTM64_ONLY(
						if (offset < 0  ||  offset > MAX_OFFSET)
							GTMASSERT;
					)
					emit_base_offset(GTM_REG_FRAME_TMP_PTR, offset);
					break;
				case TCAD_REF:
				case TVAD_REF:
				case TVAR_REF:
					offset = sa_temps_offset[opr->oprclass];
					offset -= (sa_temps[opr->oprclass] - opr->oprval.temp) * sa_class_sizes[opr->oprclass];
					GTM64_ONLY(
						if (sa_class_sizes[opr->oprclass] == 4)
						{
							next_ptr_offset = 4;
							REVERT_GENERICINST_TO_WORD(generic_inst);
						}
					)
					NON_GTM64_ONLY(
						if (offset < 0 || offset > MAX_OFFSET)
							GTMASSERT;
					)
					if (opr->oprclass == TVAR_REF)
						reg = GTM_REG_FRAME_VAR_PTR;
					else
						reg = GTM_REG_FRAME_TMP_PTR;
					emit_base_offset(reg, offset);
					if (val_output)	/* indirection */
					{
						if (GENERIC_OPCODE_LDA == generic_inst)
						{
							RISC_ONLY(code_buf[code_idx++] |= IGEN_LOAD_NATIVE_REG(trg_reg);)
							NON_RISC_ONLY(IGEN_LOAD_NATIVE_REG(trg_reg))
							if (opr->oprclass == TVAR_REF)
							{
								emit_base_offset(trg_reg, offsetof(ht_ent_mname, value));
								NON_RISC_ONLY(IGEN_LOAD_NATIVE_REG(trg_reg))
								RISC_ONLY(code_buf[code_idx++] |= IGEN_LOAD_NATIVE_REG(trg_reg);)
							}
							inst_emitted = TRUE;
						} else
						{
							RISC_ONLY(code_buf[code_idx++]
									|= IGEN_LOAD_NATIVE_REG(GTM_REG_CODEGEN_TEMP);)
							NON_RISC_ONLY(IGEN_LOAD_NATIVE_REG(GTM_REG_CODEGEN_TEMP);)
							emit_base_offset(GTM_REG_CODEGEN_TEMP, 0);
						}
					}
					break;
				case OCNT_REF:
					immediate = opr->oprval.offset;
					assert(0 <= immediate);
					assert(MAX_BRANCH_CODEGEN_SIZE > immediate);
					EMIT_TRIP_ILIT_GEN;
					inst_emitted = TRUE;
					ocnt_ref_seen = TRUE;
					ocnt_ref_opr = opr;
					break;
				default:
					GTMASSERT;
					break;
			}
			/* If we haven't emitted a finished instruction already, finish it now */
			if (!inst_emitted) {
				RISC_ONLY(code_buf[code_idx++] |= IGEN_GENERIC_REG(generic_inst, trg_reg);)
				NON_RISC_ONLY(IGEN_GENERIC_REG(generic_inst, trg_reg))
			}
			break;
		default:
			GTMASSERT;
			break;
	}
}


/*	get_arg_reg
 *
 *	Determines the argument position of the current argument and returns the number of the register to use for the
 *	value of the argument.  If it's not one of the arguments passed in machine registers, get_arg_reg defaults to the
 *	accumulator emulator register.
 *
 *	NOTE: because shrink_jmps does not always process emulated VAX instructions that generate arguments, it is crucial
 *	that get_arg_reg() and emit_push() predict the same number of instructions during the CGP_APPROX_ADDR phase as are
 *	actually generated during subsequent phases.  In order to ensure this, they emulate instruction generation backwards
 *	during the CGP_APPROX_ADDR and CGP_ADDR_OPT phases relative to the other phases.  For example:
 *
 *	CGP_APPROX_ADDR and CGP_ADDR_OPT phases:
 *		arg1 <- first argument, . . ., argN <- N th argument
 *		if more than N, series of:
 *				LOAD	GTM_REG_ACCUM, next argument
 *				STORE	GTM_REG_ACCUM, STACK_WORD_SIZE*(i-N)(sp)
 *
 *	other phases:
 *		if more than N, series of:
 *				LOAD	GTM_REG_ACCUM, next argument
 *				STORE	GTM_REG_ACCUM, STACK_WORD_SIZE*(i-N)(sp)
 *		argN <- N th argument, . . ., arg1 <- first argument
 *	where STACK_WORD_SIZE is 8(Alpha) or 4(other platforms).
 *
 *	While this technique correctly predicts the number of arguments, it does not guarantee to start any of the
 *	individual argument instruction sequences, except the first, during the CGP_APPROX_ADDR phase at the same
 *	code_reference address as it will for subsequent phases.  This is because, although it predicts (or should)
 *	the same number of instructions during the CGP_APPROX_ADDR phase for an overall sequence of argument pushes,
 *	it does not do so in the same order as subsequent phases.  Because we do not use PC-relative addressing for
 *	data on this platform, this difference should be benign (the subsequent xfer table call should be synchronized
 *	with respect to code_reference address across all phases).
 */

int	get_arg_reg(void)
{
	int	arg_reg_i;

	switch (cg_phase)
	{
		case	CGP_APPROX_ADDR:
		case	CGP_ADDR_OPT:
			if (vax_pushes_seen < MACHINE_REG_ARGS)
				arg_reg_i = GET_ARG_REG(vax_pushes_seen);
			else
				arg_reg_i = GTM_REG_ACCUM;
			break;
		case	CGP_ASSEMBLY:
		case	CGP_MACHINE:
			if (vax_pushes_seen == 0)	/* first push of a series */
				vax_number_of_arguments = next_vax_push_list();
			if (vax_number_of_arguments <= MACHINE_REG_ARGS)
				arg_reg_i = GET_ARG_REG(vax_number_of_arguments - 1);
			else
				arg_reg_i = GTM_REG_ACCUM;
			break;
		default:
			GTMASSERT;
			break;
	}
	return arg_reg_i;
}


/* VAX reg to local machine reg */
int	gtm_reg(int vax_reg)
{
	int	reg;

	switch (vax_reg & 0x0f)	/* mask out VAX register mode field */
	{
		case 0:
			reg = GTM_REG_R0;
			break;
		case 1:
			reg = GTM_REG_R1;
			break;
		case 8:
			reg = GTM_REG_FRAME_VAR_PTR;
			break;
		case 9:
			reg = GTM_REG_FRAME_TMP_PTR;
			break;
#		ifdef TRUTH_IN_REG
		case 10:
			/* The value of $TEST is maintained in r10 for the VAX GT.M
			 * implementation.  On platforms with an insufficient number of
			 * non-volatile (saved) registers, the value of $TEST is maintained
			 * only in memory; when sufficient registers are available, though,
			 * we keep $TEST in one of them.
			 */
			reg = GTM_REG_DOLLAR_TRUTH;
			break;
#		endif
		case 11:
			reg = GTM_REG_XFER_TABLE;
			break;
		case 12:
			reg = GTM_REG_FRAME_POINTER;
			break;	/* VMS ap */
		default:
			GTMASSERT;
			break;
	}
	return reg;
}


void	emit_push(int reg)
{
	int	arg_reg_i;
	int	stack_offset;

	switch (cg_phase)
	{
		case CGP_APPROX_ADDR:
		case CGP_ADDR_OPT:
			if (vax_pushes_seen >= MACHINE_REG_ARGS)
			{
				RISC_ONLY(code_idx++;)	/* for STORE instruction */
				NON_RISC_ONLY(
					assert(reg == GTM_REG_ACCUM);
					stack_offset = STACK_ARG_OFFSET((vax_number_of_arguments - MACHINE_REG_ARGS - 1));
					GEN_STORE_ARG(reg, stack_offset);	/* Store arg on stack */
				)
			}
			break;
		case CGP_ASSEMBLY:
		case CGP_MACHINE:
			if (vax_number_of_arguments <= MACHINE_REG_ARGS)
				assert(reg == GET_ARG_REG(vax_number_of_arguments - 1));
			else
			{
				assert(reg == GTM_REG_ACCUM);
				stack_offset = STACK_ARG_OFFSET((vax_number_of_arguments - MACHINE_REG_ARGS - 1));
				GEN_STORE_ARG(reg, stack_offset);	/* Store arg on stack */
			}
			break;
		default:
			GTMASSERT;
			break;
	}
	if (cg_phase == CGP_MACHINE || cg_phase == CGP_ASSEMBLY)
	{
		vax_number_of_arguments--;		/* actually, it's the number of arguments remaining */
		assert(vax_number_of_arguments >= 0);
	}
	vax_pushes_seen++;
	stack_depth++;
	return;
}


void	emit_pop(int count)
{
	int	stack_adjust;

	assert(stack_depth >= count);
	stack_depth -= count;
	/* It's possible we lost count after a jsb (see VXI_JSB). */
	if (stack_depth < 0)
		stack_depth = 0;
	return;
}


void	add_to_vax_push_list(int pushes_seen)
{	/* Make sure there's enough room */
	if (pushes_seen > MAX_ARGS)	/* user-visible max args is MAX_ARGS - 3 */
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_MAXARGCNT, 1, MAX_ARGS - 3);
	push_list_index++;
	if (push_list_index >= PUSH_LIST_SIZE)
	{
		push_list_index = 0;
		if (current_push_list_ptr->next == 0 )
		{
			current_push_list_ptr->next = (struct push_list *)malloc(SIZEOF(*current_push_list_ptr));
			current_push_list_ptr->next->next = 0;
		}
		current_push_list_ptr = current_push_list_ptr->next;
	}
	current_push_list_ptr->value[push_list_index] = pushes_seen;
}


int	next_vax_push_list(void)
{
	push_list_index++;
	if (push_list_index >= PUSH_LIST_SIZE)
	{
		push_list_index=0;
		if (current_push_list_ptr->next == 0 )
			GTMASSERT;
		current_push_list_ptr = current_push_list_ptr->next;
	}
	return (current_push_list_ptr->value[push_list_index]);
}


void	push_list_init(void)
{
	push_list_index = -1;
	if (push_list_start_ptr == 0)
	{
		push_list_start_ptr = (struct push_list *)malloc(SIZEOF(*current_push_list_ptr));
		push_list_start_ptr->next = 0;
	}
	current_push_list_ptr = push_list_start_ptr;
}


void reset_push_list_ptr(void)
{
	push_list_index = -1;
	current_push_list_ptr = push_list_start_ptr;
}


void	emit_call_xfer(int xfer)
{
	int		offset;
	unsigned char	*c;

#	ifdef DEBUG
	if (CGP_ASSEMBLY == cg_phase)
	{
		memcpy(obpt, &vdat_def[0], VDAT_DEF_SIZE);
		obpt += VDAT_DEF_SIZE;
		if (xfer < 127)
		{
			memcpy(obpt, &vdat_bdisp[0], VDAT_BDISP_SIZE);
			obpt += VDAT_BDISP_SIZE;
		} else
		{
			memcpy(obpt, &vdat_wdisp[0], VDAT_WDISP_SIZE);
			obpt += VDAT_WDISP_SIZE;
		}
		offset = (int)(xfer / SIZEOF(char *));
		for (c = (unsigned char *)xfer_name[offset]; *c ; )
			*obpt++ = *c++;
		memcpy(obpt, &vdat_r11[0], VDAT_R11_SIZE);
		obpt += VDAT_R11_SIZE;
		*obpt++ = ',';
		*obpt++ = ' ';
	}
#	endif
	assert(0 == (xfer & 0x3));
	offset = (int)(xfer / SIZEOF(char *));
#	ifdef __x86_64__
	/* Set RAX to 0 for variable argument function calls. This is part of the ABI.
	 * The RAX represents the # of floating of values being passed
	 */
	if (GTM_C_VAR_ARGS_RTN == xfer_table_desc[offset])
	{
		GEN_LOAD_IMMED(I386_REG_RAX, 0);
	}
#	endif /* __x86_64__ */
#	ifdef __ia64
        if (GTM_ASM_RTN == xfer_table_desc[offset])
        {
                GEN_XFER_TBL_CALL_FAKE(xfer);
        } else
        {
                GEN_XFER_TBL_CALL_DIRECT(xfer);

        }
#	else
	GEN_XFER_TBL_CALL(xfer);
#	endif /* __ia64 */

	/* In the normal case we will return */
	if (!ocnt_ref_seen)
		return;		/* fast test for return .. we hope */
	/* If ocnt_ref_seen is set, then we need to compute the value to be used by a recent
		OCNT_REF parameter. This parameter is (currently as of 6/2003) used by op_call, op_callsp,
		op_forlcldo, and their mprof counterparts and is the number of bytes those entry points
		should add to the return address that they will store as the return point in the new stack
		frame that they create. This parameter is basically the size of the generated code for the
		jump that follows the call to the above routines that is generates by the associated
		triples OC_CALL, OC_CALLSP, and OC_FORLCLDO respectively. Since this jump can be variable in
		size and the only other way for these routines to know what form the jump takes is to parse
		the instructions at run time, this routine in the compiler will calculate that information and
		allow it to be passed in as a parameter. The OCNT_REF handler in emit_trip() has set the
		ocnt_ref_seen flag to bring us here. We now calculate the current PC address and subtract it
		from the PC address of the next triple.
	*/
	assert(OC_CALL == current_triple->opcode || OC_CALLSP == current_triple->opcode ||
		OC_FORLCLDO == current_triple->opcode);
	offset = current_triple->exorder.fl->rtaddr - (code_reference + (code_idx * INST_SIZE));
	/* If in assembly or machine (final) phases, make sure have reasonable offset. The offset may be
		negative in the early phases so don't check during them. For other phases, put a govenor on
		the values so we don't affect the codegen sizes which can mess up shrink_trips. During the
		triple shrink phase, the triple distances can vary widely and cause the codegen to change
		sizes. Note this still allows an assert fail for 0 if a negative number was being produced.
	*/
	if (CGP_MACHINE == cg_phase || CGP_ASSEMBLY == cg_phase)
	{
		assert(0 <= offset && MAX_BRANCH_CODEGEN_SIZE > offset);
	} else
		offset = MAX(0, MIN(128, offset));
	ocnt_ref_opr->oprval.offset = offset;
	ocnt_ref_seen = FALSE;
}
