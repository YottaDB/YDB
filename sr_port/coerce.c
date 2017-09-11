/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
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

	assert ((OCT_MVAL == new_type) || (OCT_MINT == new_type) || (OCT_BOOL == new_type));
	assert (TRIP_REF == a->oprclass);
	ref = a->oprval.tref;
	old_op = ref->opcode;
	if (new_type & oc_tab[old_op].octype)
		return;
	if ((OC_COMVAL == old_op) || (OC_COMINT == old_op) || (OC_FORCENUM == old_op))
	{
		assert(TRIP_REF == ref->operand[0].oprclass);
		if ((OC_FORCENUM != old_op) || (OC_LIT == ref->operand[0].oprval.tref->opcode))
		{	/* because compiler generated literals include their numeric form, we don't need to coerce */
			assert(MV_NM & ref->operand[0].oprval.tref->operand[0].oprval.mlit->v.mvtype);
			assert(ref->operand[0].oprval.tref == ref->exorder.bl);
			dqdel(ref, exorder);
			ref = ref->operand[0].oprval.tref;
			old_op = ref->opcode;
			if (new_type & oc_tab[old_op].octype)
				return;
		}
	} else if ((OC_LIT == old_op) && (OCT_MINT == new_type))
	{
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
