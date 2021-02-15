/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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

GBLREF hash_table_str	*complits_hashtab;

LITREF octabstruct	oc_tab[];

void coerce(oprtype *a, enum octype_t new_type)
/* ensure operand (*a) is of the desired type new_type */
{

	boolean_t	litdltd;
	ht_ent_str	*litent;
	mliteral 	*lit;
	opctype		conv, old_op;
	stringkey	litkey;
	triple		*coerc, *ref;
<<<<<<< HEAD

	assert((OCT_MVAL == new_type) || (OCT_MINT == new_type) || (OCT_BOOL == new_type));
	assert(TRIP_REF == a->oprclass);
=======
	assert ((OCT_MVAL == new_type) || (OCT_MINT == new_type) || (OCT_BOOL == new_type));
	assert (TRIP_REF == a->oprclass);
>>>>>>> 451ab477 (GT.M V7.0-000)
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
<<<<<<< HEAD
			ref->opcode = OC_NOOP;			/* dqdel of OC_FORCENUM causes chain troubles */
=======
			ref->opcode = OC_NOOP;					/* dqdel of OC_FORCENUM causes chain troubles */
>>>>>>> 451ab477 (GT.M V7.0-000)
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
			if (!(++lit->rt_addr))
			{	/* completely removing this otherwise unused literal as needs to be an ILIT instead */
				if (NULL != complits_hashtab && NULL != complits_hashtab->base)
				{	/* Deleted entry is in the hash table .. remove it */
					litkey.str = lit->v.str;
					COMPUTE_HASH_STR(&litkey);
					DEBUG_ONLY(litent = lookup_hashtab_str(complits_hashtab, &litkey));
					assert(litent);	/* Literal is there .. better be found */
					assert(litent->value == (void *)lit);
					litdltd = delete_hashtab_str(complits_hashtab, &litkey);
					assert(litdltd);
				}
				dqdel(lit, que);
			}
			ref->opcode = OC_ILIT;
			ref->operand[0].oprclass = ILIT_REF;
			ref->operand[0].oprval.ilit = MV_FORCE_INTD(&(lit->v));
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
	if ((OC_COMINT == conv) || (OC_COMVAL == conv))
		coerc->operand[1] = make_ilit((mint)INIT_GBL_BOOL_DEPTH);
	*a = put_tref(coerc);
	return;
}
