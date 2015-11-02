/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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
#include "jmp_opto.h"
#include "gtmdbglvl.h"
#include "cdbg_dump.h"

LITREF octabstruct	oc_tab[];	/* op-code table */
GBLREF triple		t_orig;		/* head of triples */
GBLREF uint4		gtmDebugLevel;

#define IND_NOT_DEFINED ((unsigned char)-2)
#define JOPT_NO_OPT 1
#define JOPT_REP_JMP 2
#define JOPT_REF_NXT_TRP 3
#define NO_ENTRY ((unsigned char)-1)
#define NUM_JO_TBL_ELE 11
#define PTR_NOT_DEFINED 0

typedef struct
{
	unsigned int	opcode;
	unsigned int	index;
	unsigned int	opto_flag[NUM_JO_TBL_ELE];
} jump_opto_struct;

LITDEF readonly jump_opto_struct jump_opto_table[NUM_JO_TBL_ELE] =
{
	{	OC_JMP,		/* opcode */
		0,		/* index */
		{
			 JOPT_REP_JMP,	/* OC_JMP */		 JOPT_NO_OPT,	/* OC_JMPTSET */
			 JOPT_NO_OPT,	/* OC_JMPTCLR */	 JOPT_NO_OPT,	/* OC_JMPEQU */
			 JOPT_NO_OPT,	/* OC_JMPNEQ */		 JOPT_NO_OPT,	/* OC_JMPGTR */
			 JOPT_NO_OPT,	/* OC_JMPLEQ */		 JOPT_NO_OPT,	/* OC_JMPLSS */
			 JOPT_NO_OPT,	/* OC_JMPGEQ */		 JOPT_NO_OPT,	/* HALT */
			 JOPT_NO_OPT	/* RET */
		}
	},
	{	OC_JMPTSET,	/* opcode */
		1,		/* index */
		{
			 JOPT_REP_JMP,	/* OC_JMP */		 JOPT_REP_JMP,	/* OC_JMPTSET */
			 JOPT_REF_NXT_TRP, /* OC_JMPTCLR */	 JOPT_NO_OPT,	/* OC_JMPEQU */
			 JOPT_NO_OPT,	/* OC_JMPNEQ */		 JOPT_NO_OPT,	/* OC_JMPGTR */
			 JOPT_NO_OPT,	/* OC_JMPLEQ */		 JOPT_NO_OPT,	/* OC_JMPLSS */
			 JOPT_NO_OPT,	/* OC_JMPGEQ */		 JOPT_NO_OPT,	/* HALT */
			 JOPT_NO_OPT	/* RET */
		}
	},
	{	OC_JMPTCLR,	/* opcode */
		2,		/* index */
		{
			 JOPT_REP_JMP,	/* OC_JMP */		 JOPT_REF_NXT_TRP,	/* OC_JMPTSET */
			 JOPT_REP_JMP,	/* OC_JMPTCLR */	 JOPT_NO_OPT,	/* OC_JMPEQU */
			 JOPT_NO_OPT,	/* OC_JMPNEQ */		 JOPT_NO_OPT,	/* OC_JMPGTR */
			 JOPT_NO_OPT,	/* OC_JMPLEQ */		 JOPT_NO_OPT,	/* OC_JMPLSS */
			 JOPT_NO_OPT,	/* OC_JMPGEQ */		 JOPT_NO_OPT,	/* HALT */
			 JOPT_NO_OPT	/* RET */
		}
	},
	{	OC_JMPEQU,	/* opcode */
		3,		/* index */
		{
			 JOPT_REP_JMP,	/* OC_JMP */		 JOPT_NO_OPT,	/* OC_JMPTSET */
			 JOPT_NO_OPT,	/* OC_JMPTCLR */	 JOPT_REP_JMP,	/* OC_JMPEQU */
			 JOPT_REF_NXT_TRP, /* OC_JMPNEQ */	 JOPT_REF_NXT_TRP,	/* OC_JMPGTR */
			 JOPT_NO_OPT,	/* OC_JMPLEQ */		 JOPT_REF_NXT_TRP,	/* OC_JMPLSS */
			 JOPT_NO_OPT,	/* OC_JMPGEQ */		 JOPT_NO_OPT,	/* HALT */
			 JOPT_NO_OPT	/* RET */
		}
	},
	{	OC_JMPNEQ,	/* opcode */
		4,		/* index */
		{
			 JOPT_REP_JMP,	/* OC_JMP */		 JOPT_NO_OPT,	/* OC_JMPTSET */
			 JOPT_NO_OPT,	/* OC_JMPTCLR */	 JOPT_REF_NXT_TRP,	/* OC_JMPEQU */
			 JOPT_REP_JMP,	/* OC_JMPNEQ */		 JOPT_NO_OPT,	/* OC_JMPGTR */
			 JOPT_NO_OPT,	/* OC_JMPLEQ */		 JOPT_NO_OPT,	/* OC_JMPLSS */
			 JOPT_NO_OPT,	/* OC_JMPGEQ */		 JOPT_NO_OPT,	/* HALT */
			 JOPT_NO_OPT	/* RET */
		}
	},
	{	OC_JMPGTR,	/* opcode */
		5,		/* index */
		{
			 JOPT_REP_JMP,	/* OC_JMP */		 JOPT_NO_OPT,		/* OC_JMPTSET */
			 JOPT_NO_OPT,	/* OC_JMPTCLR */	 JOPT_REF_NXT_TRP,	/* OC_JMPEQU */
			 JOPT_REP_JMP,	/* OC_JMPNEQ */		 JOPT_REP_JMP,		/* OC_JMPGTR */
			 JOPT_REF_NXT_TRP, /* OC_JMPLEQ */	 JOPT_REF_NXT_TRP,	/* OC_JMPLSS */
			 JOPT_NO_OPT,	/* OC_JMPGEQ */		 JOPT_NO_OPT,		/* HALT */
			 JOPT_NO_OPT	/* RET */
		}
	},
	{	OC_JMPLEQ,	/* opcode */
		6,		/* index */
		{
			 JOPT_REP_JMP,	/* OC_JMP */		 JOPT_NO_OPT,	/* OC_JMPTSET */
			 JOPT_NO_OPT,	/* OC_JMPTCLR */	 JOPT_NO_OPT,	/* OC_JMPEQU */
			 JOPT_NO_OPT,	/* OC_JMPNEQ */		 JOPT_REF_NXT_TRP,	/* OC_JMPGTR */
			 JOPT_REP_JMP,	/* OC_JMPLEQ */		 JOPT_NO_OPT,	/* OC_JMPLSS */
			 JOPT_NO_OPT,	/* OC_JMPGEQ */		 JOPT_NO_OPT,	/* HALT */
			 JOPT_NO_OPT	/* RET */
		}
	},
	{	OC_JMPLSS,	/* opcode */
		7,		/* index */
		{
			 JOPT_REP_JMP,	/* OC_JMP */		 JOPT_NO_OPT,	/* OC_JMPTSET */
			 JOPT_NO_OPT,	/* OC_JMPTCLR */	 JOPT_REF_NXT_TRP,	/* OC_JMPEQU */
			 JOPT_REP_JMP,	/* OC_JMPNEQ */		 JOPT_REF_NXT_TRP,	/* OC_JMPGTR */
			 JOPT_REP_JMP,	/* OC_JMPLEQ */		 JOPT_REP_JMP,	/* OC_JMPLSS */
			 JOPT_REF_NXT_TRP,	/* OC_JMPGEQ */		 JOPT_NO_OPT,	/* HALT */
			 JOPT_NO_OPT	/* RET */
		}
	},
	{	OC_JMPGEQ,	/* opcode */
		8,		/* index */
		{
			 JOPT_REP_JMP,	/* OC_JMP */		 JOPT_NO_OPT,	/* OC_JMPTSET */
			 JOPT_NO_OPT,	/* OC_JMPTCLR */	 JOPT_NO_OPT,	/* OC_JMPEQU */
			 JOPT_NO_OPT,	/* OC_JMPNEQ */		 JOPT_NO_OPT,	/* OC_JMPGTR */
			 JOPT_NO_OPT,	/* OC_JMPLEQ */		 JOPT_REF_NXT_TRP,	/* OC_JMPLSS */
			 JOPT_REP_JMP,	/* OC_JMPGEQ */		 JOPT_NO_OPT,	/* HALT */
			 JOPT_NO_OPT	/* RET */
		}
	},
	{	OC_HALT,	/* opcode */
		9,		/* index */
		{
			 JOPT_NO_OPT,	/* OC_JMP */		 JOPT_NO_OPT,	/* OC_JMPTSET */
			 JOPT_NO_OPT,	/* OC_JMPTCLR */	 JOPT_NO_OPT,	/* OC_JMPEQU */
			 JOPT_NO_OPT,	/* OC_JMPNEQ */		 JOPT_NO_OPT,	/* OC_JMPGTR */
			 JOPT_NO_OPT,	/* OC_JMPLEQ */		 JOPT_NO_OPT,	/* OC_JMPLSS */
			 JOPT_NO_OPT,	/* OC_JMPGEQ */		 JOPT_NO_OPT,	/* HALT */
			 JOPT_NO_OPT	/* RET */
		}
	},
	{	OC_RET,		/* opcode */
		10,		/* index */
		{
			 JOPT_NO_OPT,	/* OC_JMP */		 JOPT_NO_OPT,	/* OC_JMPTSET */
			 JOPT_NO_OPT,	/* OC_JMPTCLR */	 JOPT_NO_OPT,	/* OC_JMPEQU */
			 JOPT_NO_OPT,	/* OC_JMPNEQ */		 JOPT_NO_OPT,	/* OC_JMPGTR */
			 JOPT_NO_OPT,	/* OC_JMPLEQ */		 JOPT_NO_OPT,	/* OC_JMPLSS */
			 JOPT_NO_OPT,	/* OC_JMPGEQ */		 JOPT_NO_OPT,	/* HALT */
			 JOPT_NO_OPT	/* RET */
		}
	}
};
STATICDEF	unsigned int *jo_ptr_ray[OPCODE_COUNT];
STATICDEF	unsigned int jo_ind_ray[OPCODE_COUNT];

STATICFNDCL	void jo_get_ptrs(unsigned int op);

STATICFNDEF void jo_get_ptrs(unsigned int op)
{
	const jump_opto_struct	*j, *j_top;

	for (j = &jump_opto_table[0], j_top = j + NUM_JO_TBL_ELE; j < j_top; j++)
	{
		if (j->opcode == op)
		{
			jo_ind_ray[op] = j->index;
			jo_ptr_ray[op] = (unsigned int *)&j->opto_flag[0];
			return;
		}
	}
	jo_ind_ray[op] = NO_ENTRY;
	jo_ptr_ray[op] = (unsigned int *)NO_ENTRY;
}

STATICDEF const readonly oprtype null_operand;

/************************************************************************************************************
 NOTE:	We may which to modify the lookup method at some point in the future.  B. Shear suggests nested switch
	statements. Another option is to do the lookup each time.
***********************************************************************************************************/

void jmp_opto(void)
{
	unsigned int	**clrp1, *clrp2, **clrtop1, *clrtop2, i, *p;
	tbp 		*b;
	triple		*ct, *cur_trip, *jump_trip, *next_trip, *ref_trip, *terminal_trip;
	void		get_jo_ptrs();

#	ifdef DEBUG
	/* If debug and compiler debugging is enabled, run through the triples again to show where we are jus
	 * before we modify them.
	 */
	if (gtmDebugLevel & GDL_DebugCompiler)
	{
		PRINTF(" \n\n\n\n************************************ Begin jmp_opto scan *****************************\n");
	}
#	endif
	for (clrp1 = &jo_ptr_ray[0], clrtop1 = clrp1 + OPCODE_COUNT; clrp1 < clrtop1; clrp1++)
		*clrp1 = (unsigned int *)NO_ENTRY;
	for (clrp2 = &jo_ind_ray[0], clrtop2 = clrp2 + OPCODE_COUNT; clrp2 < clrtop2; clrp2++)
		*clrp2 = NO_ENTRY;
	dqloop(&t_orig, exorder, cur_trip)
	{
		if (OC_GVSAVTARG == cur_trip->opcode)
		{	/* Look for an adjacent and therefore superfluous GVRECTARG */
			for (next_trip = cur_trip->exorder.fl;
			     oc_tab[next_trip->opcode].octype & OCT_CGSKIP;
			     next_trip = next_trip->exorder.fl)
				;
			if ((OC_GVRECTARG == next_trip->opcode)
			    && (next_trip->operand[0].oprval.tref == cur_trip)
			    && (next_trip->jmplist.que.fl == &(next_trip->jmplist)))
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
			    && (next_trip->jmplist.que.fl == &(next_trip->jmplist)))
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
			if (PTR_NOT_DEFINED == (p = jo_ptr_ray[cur_trip->opcode]))	/* note assignment */
			{
				jo_get_ptrs(cur_trip->opcode);
				p = jo_ptr_ray[cur_trip->opcode];
			}
			assert(TJMP_REF == cur_trip->operand[0].oprclass);
			jump_trip = cur_trip->operand[0].oprval.tref;
			if (IND_NOT_DEFINED == (i = jo_ind_ray[jump_trip->opcode]))	/* note assignment */
			{
				jo_get_ptrs(jump_trip->opcode);
				i = jo_ind_ray[jump_trip->opcode];
			}
			while ((IND_NOT_DEFINED != i) && (NO_ENTRY != i))
			{
				switch(p[i])
				{
					case JOPT_NO_OPT:
						i = NO_ENTRY;
						break;
					case JOPT_REF_NXT_TRP:
						if (cur_trip->src.line == jump_trip->src.line)
						{
							dqloop(&jump_trip->jmplist, que, b)
							{
								if (b->bpt = cur_trip)
								{
									dqdel(b, que);
									break;
								}
							}
							dqins(&jump_trip->exorder.fl->jmplist, que, b);
							cur_trip->operand[0].oprval.tref = jump_trip->exorder.fl;
							jump_trip = cur_trip->operand[0].oprval.tref;
							if (IND_NOT_DEFINED == (i = jo_ind_ray[jump_trip->opcode])) /* assignmnt */
							{
								jo_get_ptrs(jump_trip->opcode);
								i = jo_ind_ray[jump_trip->opcode];
							}
							COMPDBG(PRINTF("jmp_opto: JOPT_REF_NXT_TRP optimization on triple "
								       "0x"lvaddr"\n", cur_trip););
						} else
							i = NO_ENTRY;
						break;
					case JOPT_REP_JMP:
						if (cur_trip->src.line == jump_trip->src.line)
						{
							assert(TJMP_REF == jump_trip->operand[0].oprclass);
							dqloop(&jump_trip->jmplist, que, b)
							{
								if (b->bpt = cur_trip)
								{
									dqdel(b, que);
									break;
								}
							}
							dqins(&jump_trip->operand[0].oprval.tref->jmplist, que, b);
							cur_trip->operand[0] = jump_trip->operand[0];
							jump_trip = cur_trip->operand[0].oprval.tref;
							if (IND_NOT_DEFINED == (i = jo_ind_ray[jump_trip->opcode])) /* assgnmnt */
							{
								jo_get_ptrs(jump_trip->opcode);
								i = jo_ind_ray[jump_trip->opcode];
							}
							COMPDBG(PRINTF("jmp_opto: JOPT_REP_JMP optimization on triple "
								       "0x"lvaddr"\n", cur_trip););
						} else
							i = NO_ENTRY;
						break;
					default:
						GTMASSERT;
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
	 * have done to them..
	 */
	if (gtmDebugLevel & GDL_DebugCompiler)
	{
		dqloop(&t_orig, exorder, ct)
		{
			PRINTF("\n ************************ Triple Start **********************\n");
			cdbg_dump_triple(ct, 0);
		}
	}
#	endif
}
