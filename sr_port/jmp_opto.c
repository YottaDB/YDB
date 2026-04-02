/****************************************************************
 *								*
 * Copyright (c) 2001-2026 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"
#include "compiler.h"
#include "opcode.h"
#include "mdq.h"
#include "mmemory.h"
#include "jmp_opto.h"
#include "gtmdbglvl.h"
#include "cdbg_dump.h"

LITREF octabstruct	oc_tab[];	/* op-code table */
GBLREF triple		t_orig;		/* head of triples */
GBLREF uint4		gtmDebugLevel;

#define NO_ENTRY 0
#define JOPT_NO_OPT 1
#define JOPT_REP_JMP 2
#define JOPT_REF_NXT_TRP 3

STATICDEF	const unsigned char jo_ind_ray[OPCODE_COUNT] =
{
	[OC_JMP] = 1,
	[OC_JMPTSET] = 2,
	[OC_JMPTCLR] = 3,
	[OC_JMPEQU] = 4,
	[OC_JMPNEQ] = 5,
	[OC_JMPGTR] = 6,
	[OC_JMPLEQ] = 7,
	[OC_JMPLSS] = 8,
	[OC_JMPGEQ] = 9,
};

STATICDEF	const unsigned char * const jo_ptr_ray[OPCODE_COUNT] =
{
	[OC_JMP] = (unsigned char[])
	{
		JOPT_NO_OPT,	/* placeholder */
		JOPT_REP_JMP,	/* OC_JMP */		JOPT_NO_OPT,	/* OC_JMPTSET */
		JOPT_NO_OPT,	/* OC_JMPTCLR */	JOPT_NO_OPT,	/* OC_JMPEQU */
		JOPT_NO_OPT,	/* OC_JMPNEQ */		JOPT_NO_OPT,	/* OC_JMPGTR */
		JOPT_NO_OPT,	/* OC_JMPLEQ */		JOPT_NO_OPT,	/* OC_JMPLSS */
		JOPT_NO_OPT,	/* OC_JMPGEQ */
	},
	[OC_JMPTSET] = (unsigned char[])
	{
		JOPT_NO_OPT,	/* placeholder */
		JOPT_REP_JMP,	/* OC_JMP */		JOPT_REP_JMP,	/* OC_JMPTSET */
		JOPT_REF_NXT_TRP, /* OC_JMPTCLR */	JOPT_NO_OPT,	/* OC_JMPEQU */
		JOPT_NO_OPT,	/* OC_JMPNEQ */		JOPT_NO_OPT,	/* OC_JMPGTR */
		JOPT_NO_OPT,	/* OC_JMPLEQ */		JOPT_NO_OPT,	/* OC_JMPLSS */
		JOPT_NO_OPT,	/* OC_JMPGEQ */
	},
	[OC_JMPTCLR] = (unsigned char[])
	{
		JOPT_NO_OPT,	/* placeholder */
		JOPT_REP_JMP,	/* OC_JMP */		 JOPT_REF_NXT_TRP,	/* OC_JMPTSET */
		JOPT_REP_JMP,	/* OC_JMPTCLR */	 JOPT_NO_OPT,	/* OC_JMPEQU */
		JOPT_NO_OPT,	/* OC_JMPNEQ */		 JOPT_NO_OPT,	/* OC_JMPGTR */
		JOPT_NO_OPT,	/* OC_JMPLEQ */		 JOPT_NO_OPT,	/* OC_JMPLSS */
		JOPT_NO_OPT,	/* OC_JMPGEQ */
	},
	[OC_JMPEQU] = (unsigned char[])
	{
		JOPT_NO_OPT,	/* placeholder */
		JOPT_REP_JMP,	/* OC_JMP */		 JOPT_NO_OPT,	/* OC_JMPTSET */
		JOPT_NO_OPT,	/* OC_JMPTCLR */	 JOPT_REP_JMP,	/* OC_JMPEQU */
		JOPT_REF_NXT_TRP, /* OC_JMPNEQ */	 JOPT_REF_NXT_TRP,	/* OC_JMPGTR */
		JOPT_NO_OPT,	/* OC_JMPLEQ */		 JOPT_REF_NXT_TRP,	/* OC_JMPLSS */
		JOPT_NO_OPT,	/* OC_JMPGEQ */
	},
	[OC_JMPNEQ] = (unsigned char[])
	{
		JOPT_NO_OPT,	/* placeholder */
		JOPT_REP_JMP,	/* OC_JMP */		 JOPT_NO_OPT,	/* OC_JMPTSET */
		JOPT_NO_OPT,	/* OC_JMPTCLR */	 JOPT_REF_NXT_TRP,	/* OC_JMPEQU */
		JOPT_REP_JMP,	/* OC_JMPNEQ */		 JOPT_NO_OPT,	/* OC_JMPGTR */
		JOPT_NO_OPT,	/* OC_JMPLEQ */		 JOPT_NO_OPT,	/* OC_JMPLSS */
		JOPT_NO_OPT,	/* OC_JMPGEQ */
	},
	[OC_JMPGTR] = (unsigned char[])
	{
		JOPT_NO_OPT,	/* placeholder */
		JOPT_REP_JMP,	/* OC_JMP */		 JOPT_NO_OPT,		/* OC_JMPTSET */
		JOPT_NO_OPT,	/* OC_JMPTCLR */	 JOPT_REF_NXT_TRP,	/* OC_JMPEQU */
		JOPT_REP_JMP,	/* OC_JMPNEQ */		 JOPT_REP_JMP,		/* OC_JMPGTR */
		JOPT_REF_NXT_TRP, /* OC_JMPLEQ */	 JOPT_REF_NXT_TRP,	/* OC_JMPLSS */
		JOPT_NO_OPT,	/* OC_JMPGEQ */
	},
	[OC_JMPLEQ] = (unsigned char[])
	{
		JOPT_NO_OPT,	/* placeholder */
		JOPT_REP_JMP,	/* OC_JMP */		 JOPT_NO_OPT,	/* OC_JMPTSET */
		JOPT_NO_OPT,	/* OC_JMPTCLR */	 JOPT_NO_OPT,	/* OC_JMPEQU */
		JOPT_NO_OPT,	/* OC_JMPNEQ */		 JOPT_REF_NXT_TRP,	/* OC_JMPGTR */
		JOPT_REP_JMP,	/* OC_JMPLEQ */		 JOPT_NO_OPT,	/* OC_JMPLSS */
		JOPT_NO_OPT,	/* OC_JMPGEQ */
	},
	[OC_JMPLSS] = (unsigned char[])
	{
		JOPT_NO_OPT,	/* placeholder */
		JOPT_REP_JMP,	/* OC_JMP */		 JOPT_NO_OPT,	/* OC_JMPTSET */
		JOPT_NO_OPT,	/* OC_JMPTCLR */	 JOPT_REF_NXT_TRP,	/* OC_JMPEQU */
		JOPT_REP_JMP,	/* OC_JMPNEQ */		 JOPT_REF_NXT_TRP,	/* OC_JMPGTR */
		JOPT_REP_JMP,	/* OC_JMPLEQ */		 JOPT_REP_JMP,	/* OC_JMPLSS */
		JOPT_REF_NXT_TRP,	/* OC_JMPGEQ */
	},
	[OC_JMPGEQ] = (unsigned char[])
	{
		JOPT_NO_OPT,	/* placeholder */
		JOPT_REP_JMP,	/* OC_JMP */		 JOPT_NO_OPT,	/* OC_JMPTSET */
		JOPT_NO_OPT,	/* OC_JMPTCLR */	 JOPT_NO_OPT,	/* OC_JMPEQU */
		JOPT_NO_OPT,	/* OC_JMPNEQ */		 JOPT_NO_OPT,	/* OC_JMPGTR */
		JOPT_NO_OPT,	/* OC_JMPLEQ */		 JOPT_REF_NXT_TRP,	/* OC_JMPLSS */
		JOPT_REP_JMP,	/* OC_JMPGEQ */
	},
};

STATICDEF const readonly oprtype null_operand;

/************************************************************************************************************
 NOTE:	We may which to modify the lookup method at some point in the future.  B. Shear suggests nested switch
	statements. Another option is to do the lookup each time.
***********************************************************************************************************/

void jmp_opto(void)
{
	unsigned char	i;
	const unsigned char *p;
	tbp 		*b;
	triple		*ct, *cur_trip, *jump_trip, *next_trip, *ref_trip, *terminal_trip;

	COMPDBG(PRINTF("\n\n\n***************************** Begin jmp_opto scan ******************************\n"););
	dqloop(&t_orig, exorder, cur_trip)
	{
		COMPDBG(PRINTF(" ************************ Triple Start **********************\n"););
		COMPDBG(cdbg_dump_triple(cur_trip, 0););
		if (OC_GVSAVTARG == cur_trip->opcode)
		{	/* Look for an adjacent and therefore superfluous GVRECTARG */
			for (next_trip = cur_trip->exorder.fl;
			     oc_tab[next_trip->opcode].octype & OCT_CGSKIP;
			     next_trip = next_trip->exorder.fl)
				;
			if ((OC_GVRECTARG == next_trip->opcode)
			    && (next_trip->operand[0].oprval.tref == cur_trip)
			    && DQISEMPTY(next_trip->jmplist, que))
			{
				COMPDBG(PRINTF("jmp_opto: NOOPing OC_GVRECTARG opcode at triple addres 0x"lvaddr"\n", next_trip););
				next_trip->opcode = OC_NOOP;
				next_trip->operand[0].oprclass = next_trip->operand[1].oprclass = NO_REF;
				cur_trip = cur_trip->exorder.bl;	/* in case there are more than one in a row */
			}
			continue;
		}
		if (OC_GVRECTARG == cur_trip->opcode)
		{	/* Look for a second effectively adjacent GVRECTARG that duplicates this one */
			for (next_trip = cur_trip->exorder.fl;
			     oc_tab[next_trip->opcode].octype & OCT_CGSKIP;
			     next_trip = next_trip->exorder.fl)
				;
			if ((OC_GVRECTARG == next_trip->opcode)
			    && (next_trip->operand[0].oprval.tref == cur_trip->operand[0].oprval.tref)
			    && DQISEMPTY(next_trip->jmplist, que))
			{
				COMPDBG(PRINTF("jmp_opto: NOOPing OC_GVRECTARG opcode at triple addres 0x"lvaddr"\n", next_trip););
				next_trip->opcode = OC_NOOP;
				next_trip->operand[0].oprclass = next_trip->operand[1].oprclass = NO_REF;
				cur_trip = cur_trip->exorder.bl;	/* in case there are more than one in a row */
			}
			continue;
		}
		if ((oc_tab[cur_trip->opcode].octype & OCT_JUMP)
			&& (OC_CALL != cur_trip->opcode) && (OC_CALLSP != cur_trip->opcode))
		{
			assert(OPCODE_COUNT > cur_trip->opcode);
			p = jo_ptr_ray[cur_trip->opcode];
			assert(TJMP_REF == cur_trip->operand[0].oprclass);
			jump_trip = cur_trip->operand[0].oprval.tref;
			i = jo_ind_ray[jump_trip->opcode];
			while (NO_ENTRY != i && cur_trip->src.line == jump_trip->src.line)
			{
				switch(p[i])
				{
					case JOPT_NO_OPT:
						i = NO_ENTRY;
						break;
					case JOPT_REF_NXT_TRP:
						assert(NULL != jump_trip->jmplist);
						dqloop(jump_trip->jmplist, que, b)
						{
							if ((b->bpt = cur_trip))
							{
								dqdel(b, que);
								break;
							}
						}
						if (NULL == jump_trip->exorder.fl->jmplist)
						{
							jump_trip->exorder.fl->jmplist = (tbp *)mcalloc(SIZEOF(tbp));
							dqinit(jump_trip->exorder.fl->jmplist, que);
						}
						dqins(jump_trip->exorder.fl->jmplist, que, b);
						cur_trip->operand[0].oprval.tref = jump_trip->exorder.fl;
						jump_trip = cur_trip->operand[0].oprval.tref;
						i = jo_ind_ray[jump_trip->opcode];
						COMPDBG(PRINTF("jmp_opto: JOPT_REF_NXT_TRP optimization on triple "
								"0x"lvaddr"\n", cur_trip););
						break;
					case JOPT_REP_JMP:
						assert(TJMP_REF == jump_trip->operand[0].oprclass);
						assert(NULL != jump_trip->operand[0].oprval.tref->jmplist);
						assert(NULL != jump_trip->jmplist);
						dqloop(jump_trip->jmplist, que, b)
						{
							if ((b->bpt = cur_trip))
							{
								dqdel(b, que);
								break;
							}
						}
						dqins(jump_trip->operand[0].oprval.tref->jmplist, que, b);
						cur_trip->operand[0] = jump_trip->operand[0];
						jump_trip = cur_trip->operand[0].oprval.tref;
						i = jo_ind_ray[jump_trip->opcode];
						COMPDBG(PRINTF("jmp_opto: JOPT_REP_JMP optimization on triple "
								"0x"lvaddr"\n", cur_trip););
						break;
					default:
						assertpro(FALSE && p[i]);
						break;
				} /* switch */
			} /* while  */
			terminal_trip = cur_trip->exorder.fl;
			while ((oc_tab[cur_trip->opcode].octype & OCT_JUMP)
			       && (OC_CALL != cur_trip->opcode) && (OC_CALLSP != cur_trip->opcode)
			       && (TJMP_REF == cur_trip->operand[0].oprclass))
			{
				for ((ref_trip = cur_trip->operand[0].oprval.tref);
				     (oc_tab[ref_trip->opcode].octype & OCT_CGSKIP);
				     (ref_trip = ref_trip->exorder.fl))
					;
				if (ref_trip == terminal_trip)
				{
					cur_trip->opcode = OC_NOOP;
					cur_trip->operand[0] = null_operand;
					COMPDBG(PRINTF("jmp_opto: Triple removed at address 0x"lvaddr"\n", cur_trip););
					cur_trip = cur_trip->exorder.bl;
				} else
					break;
			}
			cur_trip = terminal_trip->exorder.bl;
		} /* if  */
	} /* dqloop */
#	ifdef DEBUG
	/* If debug and compiler debugging is enabled, run through the triples again to show what we
	 * have done to them.
	 */
	if (gtmDebugLevel & GDL_DebugCompiler)
	{
		PRINTF(" \n\n\n\n****************************** After jmp_opto scan *****************************\n");
		cdbg_dump_triple_all();
	}
#	endif
}
