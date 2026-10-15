/****************************************************************
 *								*
 * Copyright (c) 2026 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Indirection whose source string names a local variable with simple subscripts, such as x("a",5,y), is common in code
 * that walks a tree of unknown shape, and the string is often different on every call so the indirection cache does not
 * help. These routines recognize that form and locate the node directly, so the caller can skip compiling the string.
 * Anything not of that form (globals, intrinsic special variables, expressions, subscripted or undefined variables used as
 * subscripts, string literals with embedded quotes, non-canonical numbers) is left to the compiler, which also takes care
 * of all errors.
 */

#include "mdef.h"

#include "gtm_ctype.h"
#include "lv_val.h"
#include "hashtab_mname.h"
#include <rtnhdr.h>
#include "mv_stent.h"
#include "callg.h"
#include "op.h"
#include "compiler.h"
#include "opcode.h"
#include "glvn_pool.h"
#include "stringpool.h"
#include "indr_fast_lvn.h"

GBLREF	mv_stent	*mv_chain;
GBLREF	symval		*curr_symval;
GBLREF	spdesc		stringpool;

#define IS_NAME_START(CH)	(('%' == (CH)) || ISALPHA_ASCII(CH))
#define MAX_NUM_SUB_LEN		18	/* longer unquoted numbers are left to the compiler, which deals with their precision */

/* The result of the last op_indname, which a read on the fast path can give back to the stringpool once done with it */
static mstr		recorded_name;
static unsigned int	recorded_name_gcols;

typedef struct
{
	char	*addr;
	int	len;
	lv_val	*var;	/* non-NULL if the subscript is a variable */
} indr_fast_sub;

/* Returns the defined unsubscripted local variable named NAME, or NULL */
static lv_val *defined_var(char *addr, int len)
{
	var_tabent	key;
	ht_ent_mname	*tabent;

	key.var_name.addr = addr;
	key.var_name.len = len;
	COMPUTE_HASH_MNAME(&key);
	tabent = lookup_hashtab_mname(&curr_symval->h_symtab, &key);
	if ((NULL == tabent) || (NULL == tabent->value) || !LV_IS_VAL_DEFINED(tabent->value))
		return NULL;
	return (lv_val *)tabent->value;
}

/* Returns TRUE if SRC is a local variable name optionally followed by subscripts, each of which is a canonical number, a
 * string literal or a defined unsubscripted local variable. In that case, LVN is filled in, with the name and subscript
 * values held in MVST_MVAL entries pushed on the M stack (so a garbage collection keeps them intact). The caller must call
 * indr_fast_lvn_done() when it no longer needs them. Returns FALSE, having pushed nothing, otherwise.
 */
boolean_t indr_fast_lvn_parse(mval *src, indr_fast_lvn *lvn)
{
	indr_fast_sub	sub[MAX_LVSUBSCRIPTS];
	char		*cp, *top, *start, *name;
	int		i, namelen, nsubs;
	mval		*mv, num;
	boolean_t	plain;

	assert(MV_IS_STRING(src));
	cp = name = src->str.addr;
	top = cp + src->str.len;
	if ((cp == top) || !IS_NAME_START(*cp))
		return FALSE;
	for (cp++; (cp < top) && ISALNUM_ASCII(*cp); cp++)
		;
	namelen = INTCAST(cp - name);
	if (MAX_MIDENT_LEN < namelen)
		return FALSE;
	nsubs = 0;
	if (cp < top)
	{
		if ('(' != *cp++)
			return FALSE;
		for ( ; ; )
		{
			if ((cp == top) || (MAX_LVSUBSCRIPTS == nsubs))
				return FALSE;
			start = cp;
			sub[nsubs].var = NULL;
			if ('"' == *cp)
			{	/* A doubled quote fails the check for a following ',' or ')' below */
				for (cp++; (cp < top) && ('"' != *cp); cp++)
					;
				if (cp == top)
					return FALSE;
				sub[nsubs].addr = start + 1;
				sub[nsubs].len = INTCAST(cp - start - 1);
				cp++;
			} else if (ISDIGIT_ASCII(*cp) || ('.' == *cp) || ('-' == *cp))
			{
				for (cp++; (cp < top) && ISDIGIT_ASCII(*cp); cp++)
					;
				plain = (ISDIGIT_ASCII(*start) && (('0' != *start) || (1 == (cp - start))));
				for ( ; (cp < top) && (ISDIGIT_ASCII(*cp) || ('.' == *cp)); cp++)
					plain = FALSE;
				sub[nsubs].addr = start;
				sub[nsubs].len = INTCAST(cp - start);
				if (MAX_NUM_SUB_LEN < sub[nsubs].len)
					return FALSE;
				if (!plain)
				{	/* Anything but an unsigned integer without leading zeros needs the full check */
					num.mvtype = MV_STR;
					num.str.addr = start;
					num.str.len = sub[nsubs].len;
					if (!val_iscan(&num))
						return FALSE;
				}
			} else if (IS_NAME_START(*cp))
			{
				for (cp++; (cp < top) && ISALNUM_ASCII(*cp); cp++)
					;
				if ((MAX_MIDENT_LEN < (cp - start)) || (NULL == (sub[nsubs].var = defined_var(start, INTCAST(cp - start)))))
					return FALSE;
			} else
				return FALSE;
			nsubs++;
			if (cp == top)
				return FALSE;
			if (')' == *cp)
			{
				if (++cp != top)
					return FALSE;
				break;
			}
			if (',' != *cp++)
				return FALSE;
		}
	}
	PUSH_MV_STENT(MVST_MVAL);
	mv = lvn->name = &mv_chain->mv_st_cont.mvs_mval;
	mv->mvtype = MV_STR;
	mv->str.addr = name;
	mv->str.len = namelen;
	for (i = 0; i < nsubs; i++)
	{
		PUSH_MV_STENT(MVST_MVAL);
		mv = lvn->subs[i] = &mv_chain->mv_st_cont.mvs_mval;
		if (NULL != sub[i].var)
		{
			*mv = sub[i].var->v;
			/* Only a subscripted node can be an alias container, and a subscript variable here is unsubscripted,
			 * so MV_ALIASCONT is never set. Clearing it is not strictly necessary, but is kept for consistency
			 * with the other places that copy the value of a variable.
			 */
			mv->mvtype &= ~MV_ALIASCONT;
		} else
		{
			mv->mvtype = MV_STR;
			mv->str.addr = sub[i].addr;
			mv->str.len = sub[i].len;
		}
	}
	lvn->nsubs = nsubs;
	return TRUE;
}

/* Returns the node named by the base variable and the first DEPTH subscripts of LVN, or NULL if it does not exist */
lv_val *indr_fast_lvn_node(indr_fast_lvn *lvn, int depth)
{
	gparam_list	plist;
	int		i;
	var_tabent	key;
	ht_ent_mname	*tabent;

	assert(depth <= lvn->nsubs);
	key.var_name = lvn->name->str;
	COMPUTE_HASH_MNAME(&key);
	tabent = lookup_hashtab_mname(&curr_symval->h_symtab, &key);
	if ((NULL == tabent) || (NULL == tabent->value))
		return NULL;
	if (0 == depth)
		return (lv_val *)tabent->value;
	plist.n = depth + 1;
	plist.arg[0] = tabent->value;
	for (i = 0; i < depth; i++)
		plist.arg[i + 1] = lvn->subs[i];
	return (lv_val *)callg((callgfnptr)op_srchindx, &plist);
}

/* Saves the reference in LVN in glvn pool slot SLOT as OC_SHARESLOT and OC_SAVLVN in compiled code would: the variable
 * name as a string, which op_rfrshlvn looks up again later, and a copy of each subscript value. It stores the name parsed
 * from the source string. It cannot call op_savlvn, which finds the name by searching the variable table of the current
 * frame for the lv_val: no frame is pushed for the indirection here, so that is the caller's frame, which need not name
 * the variable, and the search would give an empty name. Returns TRUE, or FALSE, having done nothing, if the base variable
 * does not exist, leaving the compiled code to create it.
 */
boolean_t indr_fast_lvn_save(indr_fast_lvn *lvn, uint4 slot)
{
	glvn_pool_entry	*slotp;
	int		i;
	lvname_info	*lvn_info;
	mval		*m;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (NULL == indr_fast_lvn_node(lvn, 0))
		return FALSE;
	op_shareslot(slot, OC_SAVLVN);
	ENSURE_GLVN_POOL_SPACE(1 + lvn->nsubs);
	GET_GLVN_POOL_STATE(slotp, m);
	slotp->lvname = m;
	*m = *lvn->name;
	(TREF(glvn_pool_ptr))->mval_top++;
	if (!IS_IN_STRINGPOOL(m->str.addr, m->str.len))
		s2pool(&m->str);
	m++;
	lvn_info = (lvname_info *)&slotp->glvn_info;
	lvn_info->total_lv_subs = 1 + lvn->nsubs;
	for (i = 0; i < lvn->nsubs; i++, m++)
	{
		*m = *lvn->subs[i];
		lvn_info->lv_subs[i] = m;
		(TREF(glvn_pool_ptr))->mval_top++;
		if (MV_IS_STRING(m) && !IS_IN_STRINGPOOL(m->str.addr, m->str.len))
			s2pool(&m->str);
	}
	return TRUE;
}

/* Records DST, the name op_indname just built at the top of the stringpool */
void indr_fast_lvn_record_name(mval *dst)
{
	recorded_name = dst->str;
	recorded_name_gcols = stringpool.gcols;
}

/* Called by a read on the fast path once done with SRC. If SRC is the name op_indname last built, it is the compiler temporary
 * holding that result and nothing else refers to it, so if it is still at the top of the stringpool (no garbage collection and
 * no other allocation since), give its space back. Empty SRC too, since a garbage collection would otherwise find it pointing
 * past the top of the stringpool.
 */
void indr_fast_lvn_release(mval *src)
{
	if ((src->str.addr == recorded_name.addr) && (src->str.len == recorded_name.len)
			&& (stringpool.gcols == recorded_name_gcols)
			&& (((unsigned char *)src->str.addr + src->str.len) == stringpool.free))
	{
		stringpool.free = (unsigned char *)src->str.addr;
		src->str.len = 0;
	}
	recorded_name.addr = NULL;
}

/* Pops the M stack entries pushed by indr_fast_lvn_parse() */
void indr_fast_lvn_done(indr_fast_lvn *lvn)
{
	int	i;

	for (i = 0; i <= lvn->nsubs; i++)
		POP_MV_STENT();
}
