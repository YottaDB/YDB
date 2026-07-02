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

#include "compiler.h"
#include "opcode.h"
#include "mdq.h"
#include "mvalconv.h"
#include "hashtab_str.h"
#include "gcol_list.h"

GBLREF hash_table_str	*complits_hashtab;

LITREF octabstruct	oc_tab[];

void coerce(oprtype *a, unsigned short new_type)
/* ensure operand (*a) is of the desired type new_type */
{

	boolean_t	litdltd;
	ht_ent_str	*litent;
	mliteral 	*lit;
	opctype		conv, old_op;
	stringkey	litkey;
	triple		*coerc, *ref;
	int4		ilit;
	assert ((OCT_MVAL == new_type) || (OCT_MINT == new_type) || (OCT_BOOL == new_type));
	assert (TRIP_REF == a->oprclass);
	ref = a->oprval.tref;
	old_op = ref->opcode;
	if (new_type & oc_tab[old_op].octype)
		return;
	switch (old_op)
	{
		case OC_FORCENUM:
			if ((ref->operand[0].oprval.tref != ref->exorder.bl) || (OC_LIT != ref->operand[0].oprval.tref->opcode))
				break;						/* WARNING possible fallthrough */
		case OC_COMVAL:
		case OC_COMINT:
			assert(TRIP_REF == ref->operand[0].oprclass);
			if (OC_LIT == ref->operand[0].oprval.tref->opcode)
			{	/* compiler generated literals should include their numeric form - no need to coerce */
				MV_FORCE_NUMD(&ref->operand[0].oprval.tref->operand[0].oprval.mlit->v);
			}
			ref->opcode = OC_NOOP;					/* dqdel of OC_FORCENUM causes chain troubles */
			ref->operand[0].oprclass = NO_REF;
			ref = ref->operand[0].oprval.tref;
			old_op = ref->opcode;
			if (new_type & oc_tab[old_op].octype)
				return;
			break;
		case OC_LIT:
			if (OCT_MINT != new_type)
				break;
			lit = ref->operand[0].oprval.mlit;
			ilit = MV_FORCE_INTD(&(lit->v));
			unuse_literal(&(lit->v));
			ref->opcode = OC_ILIT;
			ref->operand[0].oprclass = ILIT_REF;
			ref->operand[0].oprval.ilit = ilit;
			return;
		default:
			break;
	}
	if (OCT_BOOL == new_type)
		conv = OC_COBOOL;
	else if (OCT_MINT == new_type)
		conv = OC_COMINT;
	else
		conv = OC_COMVAL;
	coerc = newtriple(conv);
	coerc->operand[0] = put_tref(ref);
	*a = put_tref(coerc);
	return;
}
