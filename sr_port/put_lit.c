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

#include "mdq.h"
#include "hashtab_str.h"
#include "compiler.h"
#include "opcode.h"
#include "mmemory.h"

GBLREF int		mlitmax;
GBLREF mliteral 	literal_chain;
GBLREF hash_table_str	*complits_hashtab;

oprtype put_lit(mval *x)
{
	mliteral	*a;
	triple		*ref;
	boolean_t	usehtab, added;
	stringkey	litkey;
	ht_ent_str	*litent;

	assert(MV_DEFINED(x));
	MV_FORCE_STR(x);
	DEBUG_ONLY(litent = NULL);
	ref = newtriple(OC_LIT);
	ref->operand[0].oprclass = MLIT_REF;
	/* Multiple reasons to use hashtab since coerce which processes integer parms to functions will
	   actually *remove* literals if they were just put on so we don't want to convert to hashtab
	   then have that literal and/or some others yanked to pull us back under the count as that would
	   confuse things mightily.
	*/
	usehtab = (LIT_HASH_CUTOVER < mlitmax) || (complits_hashtab && complits_hashtab->base);
	if (!usehtab)
	{	/* Brute force scan under cutover .. should include all intrinsics */
		dqloop(&literal_chain, que, a)
			if (is_equ(x, &(a->v)))
			{
				a->rt_addr--;
				ref->operand[0].oprval.mlit = a;
				return put_tref(ref);
			}
	} else
	{	/* Use hash table -- load it up if haven't created it yet */
		if (!complits_hashtab)
		{
			complits_hashtab = (hash_table_str *)malloc(SIZEOF(hash_table_str));
			complits_hashtab->base = NULL;
		}
		if (!complits_hashtab->base)
		{	/* Need to initialize hash table and load it with the elements so far */
			init_hashtab_str(complits_hashtab, LIT_HASH_CUTOVER * 2, HASHTAB_NO_COMPACT, HASHTAB_NO_SPARE_TABLE);
			assert(complits_hashtab->base);
			dqloop(&literal_chain, que, a)
			{
				litkey.str = a->v.str;
				COMPUTE_HASH_STR(&litkey);
				added = add_hashtab_str(complits_hashtab, &litkey, a, &litent);
				assert(added);
				assert(litent->value);
				assert(litent->key.str.addr == ((mliteral *)litent->value)->v.str.addr);
			}
		}
		/* Set the hash value in this element */
		litkey.str = x->str;
		COMPUTE_HASH_STR(&litkey);
		added = add_hashtab_str(complits_hashtab, &litkey, NULL, &litent);
		if (!added)
		{	/* Hash entry exists for this literal */
			a = (mliteral *)litent->value;
			assert(a);
			assert(MV_DEFINED(&(a->v)));
			assert(is_equ(x, &(a->v)));
			a->rt_addr--;
			ref->operand[0].oprval.mlit = a;
			return put_tref(ref);
		}
	}
	ref->operand[0].oprval.mlit = a = (mliteral *)mcalloc(SIZEOF(mliteral));
	dqins(&literal_chain, que, a);
	a->rt_addr = -1;
	a->v = *x;
	if (usehtab)
	{	/* Now that new mlit is created, place it in created hashtab entry */
		assert(litent);
		litent->value = a;
		assert(litent->key.str.addr == ((mliteral *)litent->value)->v.str.addr);
		assert(litent->key.str.len == ((mliteral *)litent->value)->v.str.len);
	}
	mlitmax++;
	return put_tref(ref);
}
