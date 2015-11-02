/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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

void coerce(oprtype *a,unsigned short new_type)
{

	mliteral 	*lit;
	opctype		conv, old_op;
	triple		*ref, *coerc;
	stringkey	litkey;
	ht_ent_str	*litent;
	boolean_t	litdltd;

	assert (new_type == OCT_MVAL || new_type == OCT_MINT || new_type == OCT_BOOL);
	assert (a->oprclass == TRIP_REF);
	ref = a->oprval.tref;
	old_op = ref->opcode;
	if (new_type & oc_tab[old_op].octype)
		return;
	if (old_op == OC_COMVAL || old_op == OC_COMINT)
	{
		dqdel(ref,exorder);
		ref = ref->operand[0].oprval.tref;
		old_op = ref->opcode;
		if (new_type & oc_tab[old_op].octype)
			return;
	} else if (OC_LIT == old_op && OCT_MINT == new_type)
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
	if (new_type == OCT_BOOL)
		conv = OC_COBOOL;
	else if (new_type == OCT_MINT)
		conv = OC_COMINT;
	else
		conv = OC_COMVAL;
	coerc = newtriple(conv);
	coerc->operand[0] = put_tref(ref);
	*a = put_tref(coerc);
	return;
}
