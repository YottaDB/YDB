#include "mdef.h"

#include "mdq.h"
#include "hashtab_str.h"
#include "compiler.h"
#include "opcode.h"
#include "mmemory.h"

GBLREF mliteral 	literal_chain;
GBLREF hash_table_str	*complits_hashtab;

boolean_t unuse_literal(mval *x)
{
	mliteral	*a = NULL;
	stringkey	litkey;
	ht_ent_str	*litent;
	bool		in_hashtab = FALSE;
	if (complits_hashtab && complits_hashtab->base)
	{
		litkey.str = x->str;
		COMPUTE_HASH_STR(&litkey);
		if (NULL != (litent = lookup_hashtab_str(complits_hashtab, &litkey)))
		{
			a = (mliteral *)litent->value;
			assert(a->reference_count != 0);
			a->reference_count -= 1;
			in_hashtab = TRUE;
		}
	} else
	{
		dqloop(&literal_chain, que, a)
		{
			if (is_equ(x, &(a->v)))
			{
				assert(a->reference_count != 0);
				a->reference_count -= 1;
				break;
			}
		}
	}
	/* The first assert here covers the case of no literal in the hashtab, the second in the literal chain */
	assert(a != NULL); /* ATTEMPT TO REMOVE MVAL NOT IN HASHTABLE; THIS WAS CALLED IN ERROR */
	assert(a != &literal_chain); /* This probably means you attempted to remove a literal not in the literal chain */
	if (a->reference_count == 0)
	{
		/* Remove mval */
		if (in_hashtab)
			delete_hashtab_ent_str(complits_hashtab, litent);
		dqdel(a, que);
		return TRUE;
	}
	return FALSE;
}
