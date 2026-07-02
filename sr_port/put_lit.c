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

#include "mdq.h"
#include "hashtab_str.h"
#include "compiler.h"
#include "opcode.h"
#include "mmemory.h"
#include "gcol_list.h"

GBLREF hash_table_str	*complits_hashtab;
GBLREF int		mlitmax;
GBLREF mliteral 	literal_chain;
oprtype put_lit(mval *x)
{
	return put_lit_s(x, NULL);
}

oprtype put_lit_s(mval *x, triple *dst_triple)
{
	mliteral	*a;
	triple		*ref;
	boolean_t	usehtab, added;
	stringkey	litkey = {{{{0}}}};
	ht_ent_str	*litent;
	unsigned int	gcols;
	mval		lcl_mval = {{0}};

	assert(MV_DEFINED(x));
	MV_FORCE_STR(x);
	if (MV_IS_CANONICAL(x))
		MV_FORCE_NUM(x);
	DEBUG_ONLY(litent = NULL);
	if (dst_triple == NULL)
		ref = newtriple(OC_LIT);
	else
		ref = dst_triple;
	ref->operand[0].oprclass = MLIT_REF;
	/* Multiple reasons to use hashtab since coerce which processes integer parms to functions will
	 * actually *remove* literals if they were just put on so we don't want to convert to hashtab
	 * then have that literal and/or some others yanked to pull us back under the count as that would
	 * confuse things mightily.
	 */
	assert(x->mvtype & MV_STR);
	if (x->str.len && !glist_str_in_stringpool(&(x->str)))
	{
		lcl_mval.umval = x->umval;
		x = &lcl_mval;
		s2pool(&x->str);
	}
	usehtab = (LIT_HASH_CUTOVER < mlitmax) || (complits_hashtab && complits_hashtab->base);
	if (!usehtab)
	{	/* Brute force scan up to cutover to hash .. should include all intrinsics */
		dqloop(&literal_chain, que, a)
		{
			assert(glist_str_in_sync(&a->v.str));
			if (is_equ(x, &(a->v)))
			{
				a->rt_addr--;
				ref->operand[0].oprval.mlit = a;
				a->reference_count += 1;
				return put_tref(ref);
			}
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
				assert(glist_str_in_sync(&a->v.str));
				DBG_START_NO_GCOLS(gcols);
				/* No need to protect since litkey is copied and protected if necessary into internally-held
				 * key in the complits_hashtab, and there can be no garbage collection before the add_hashtab
				 * succeeds.
				 */
				litkey.str.umstr = a->v.str.umstr;
				COMPUTE_HASH_STR(&litkey);
				added = add_hashtab_str(complits_hashtab, &litkey, a, &litent);
				assert(added);
				assert(litent->value);
				assert(litent->key.str.addr == ((mliteral *)litent->value)->v.str.addr);
				DBG_END_NO_GCOLS(gcols);
			}
		}
		/* Set the hash value in this element */
		litkey.str.umstr = x->str.umstr;
		COMPUTE_HASH_STR(&litkey);
		added = add_hashtab_str(complits_hashtab, &litkey, NULL, &litent);
		if (!added)
		{	/* Hash entry exists for this literal */
			a = (mliteral *)litent->value;
			assert(a);
			assert(MV_DEFINED(&(a->v)));
			assert(is_equ(x, &(a->v)));
			assert(a->reference_count);
			a->rt_addr--;
			ref->operand[0].oprval.mlit = a;
			a->reference_count += 1;
			return put_tref(ref);
		}
	}
	ref->operand[0].oprval.mlit = a = (mliteral *)mcalloc(SIZEOF(mliteral));
	glist_first_init_str(&a->v.str);
	a->reference_count = 1;
	dqins(&literal_chain, que, a);
	a->rt_addr = -1;
	a->v.umval = x->umval;
	glist_sync_str(&a->v.str);
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
