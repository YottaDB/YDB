/****************************************************************
 *								*
 * Copyright (c) 2020 Fidelity National Information		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef GVT_INLINE_INCLUDED
#define GVT_INLINE_INCLUDED

#include "gv_xform_key.h"

static inline gv_key *gvkey_init(gv_key *gvkey, int4 keysize)
{
	GBLREF gv_key	*gv_altkey;
	GBLREF gv_key	*gv_currkey;
	gv_key		*new_KEY, *old_KEY;
	int4		keySZ;
	DEBUG_ONLY(DCL_THREADGBL_ACCESS);

	DEBUG_ONLY(SETUP_THREADGBL_ACCESS);
	old_KEY = gvkey;
	keySZ = keysize;
	/* KEYSIZE should have been the output of a DBKEYSIZE command so
	 * should be a multiple of 4. Assert that.
	 */
	assert(ROUND_UP2(keySZ, 4) == keySZ);
	new_KEY = (gv_key *)malloc(SIZEOF(gv_key) + keySZ + 1);
	assert((DBKEYSIZE(MAX_KEY_SZ) == keysize)
		|| ((gvkey != gv_currkey) && (gvkey != gv_altkey)));
	if ((NULL != old_KEY) && (PREV_KEY_NOT_COMPUTED != old_KEY->end))
	{
		/* Don't call GVKEY_INIT twice for same key. The only exception
		 * is if we are called from COPY_PREV_KEY_TO_GVT_CLUE in a
		 * restartable situation but TREF(donot_commit) should have
		 * been set to a special value then so check that.
		 */
		assert(TREF(donot_commit) | DONOTCOMMIT_COPY_PREV_KEY_TO_GVT_CLUE);
		assert(keysize >= old_KEY->top);
		assert(old_KEY->top > old_KEY->end);
		memcpy(new_KEY, old_KEY, SIZEOF(gv_key) + old_KEY->end + 1);
		free(old_KEY);
	} else
	{
		new_KEY->base[0] = '\0';
		new_KEY->end = 0;
		new_KEY->prev = 0;
	}
	new_KEY->top = keySZ;
	return new_KEY;
}

static inline void dbg_check_gvtarget_integrity(gv_namehead *gvt)
{
	int			keysize, partial_size;
	GBLREF	boolean_t	dse_running;

	if (NULL != gvt->gd_csa->nl)
	{	/* csa->nl is cleared when a statsDB is closed due to opt-out so use as flag if DB is open or not */
		keysize = gvt->gd_csa->hdr->max_key_size;
		keysize = DBKEYSIZE(keysize);
		partial_size = SIZEOF(gv_namehead) + 2 * SIZEOF(gv_key) + 3 * keysize;
		/* DSE could change the max_key_size dynamically so account for it in the below assert */
		if (!dse_running)
		{
			assert(gvt->gvname.var_name.addr == (char *)gvt + partial_size);
			assert((char *)gvt->first_rec == ((char *)&gvt->clue + SIZEOF(gv_key) + keysize));
			assert((char *)gvt->last_rec  == ((char *)gvt->first_rec + SIZEOF(gv_key) + keysize));
			assert(gvt->clue.top == keysize);
		}
		assert(gvt->clue.top == gvt->first_rec->top);
		assert(gvt->clue.top == gvt->last_rec->top);
	}
}

static inline void copy_prev_key_to_gvt_clue(gv_namehead *gvt, boolean_t expand_prev_key)
{
	GBLREF gv_key	*gv_altkey;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;

	if (expand_prev_key)
	{	/* gv_altkey has the previous key. Store it in clue for future clue-based searches */
		if (NULL == gvt->prev_key)
			GVKEY_INIT(gvt->prev_key, gvt->clue.top);
		if (gv_altkey->end >= gvt->prev_key->top)
		{	/* Note that this is possible in case of concurrency issues (i.e. we are in
			 * a restartable situation (see comment at bottom of gvcst_expand_key.c which
			 * talks about a well-formed key. Since we cannot easily signal a restart here,
			 * we reallocate to ensure the COPY_KEY does not cause a buffer overflow and
			 * the caller will eventually do the restart.
			 */
			DEBUG_ONLY(TREF(donot_commit) |= DONOTCOMMIT_COPY_PREV_KEY_TO_GVT_CLUE;)
			GVKEY_INIT(gvt->prev_key, DBKEYSIZE(gv_altkey->end));
		}
		DBG_CHECK_GVKEY_VALID(gv_altkey, DONOTCOMMIT_COPY_PREV_KEY_TO_GVT_CLUE);
		COPY_KEY(gvt->prev_key, gv_altkey);
	} else if (NULL != gvt->prev_key)
	{
		assert(PREV_KEY_NOT_COMPUTED < (1 << (SIZEOF(gv_altkey->end) * 8)));
		gvt->prev_key->end = PREV_KEY_NOT_COMPUTED;
	}
}

static inline void copy_curr_and_prev_key_to_gvtarget_clue(gv_namehead *gvt, gv_key *gvkey, boolean_t expand_prev_key)
{
	GBLREF gv_key	*gv_altkey;
	int		keyend;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	keyend = gvkey->end;
	if (gvt->clue.top <= keyend)
	{	/* Possible only if GVT corresponds to a global that spans multiple
		 * regions. For example, a gvcst_spr_* function could construct a
		 * gv_currkey starting at one spanned region and might have to do a
		 * gvcst_* operation on another spanned region with a max-key-size
		 * that is smaller than gv_currkey->end. In that case, copy only the
		 * portion of gv_currkey that will fit in the gvt of the target region.
		 */
		assert(TREF(spangbl_seen));
		keyend = gvt->clue.top - 1;
		memcpy(((gv_key *)&(gvt->clue))->base, gvkey->base, keyend - 1);
		((gv_key *)&(gvt->clue))->base[keyend - 1] = KEY_DELIMITER;
		((gv_key *)&(gvt->clue))->base[keyend] = KEY_DELIMITER;
	} else
	{
		assert(KEY_DELIMITER == gvkey->base[keyend]);
		assert(KEY_DELIMITER == gvkey->base[keyend - 1]);
		memcpy(((gv_key *)&(gvt->clue))->base, gvkey->base, keyend + 1);
	}
	gvt->clue.end = keyend;
	/* No need to maintain unused GVT->clue.prev */
	copy_prev_key_to_gvt_clue(gvt, expand_prev_key);
	dbg_check_gvtarget_integrity(gvt);
}

/* Replace a NULL subscript at the end with the maximum possible subscript
 * that could exist in the database for this global name. Used by $ZPREVIOUS, $QUERY(gvn,-1) etc.
 */

static inline void gv_append_max_subs_key(gv_key *gvkey, gv_namehead *gvt)
{
	GBLREF gd_region	*gv_cur_region;
	int			lastsubslen, keysize;
	unsigned char		*ptr;

	assert(gvt->clue.top || (NULL == gvt->gd_csa));
	assert(!gvt->clue.top || (NULL != gvt->gd_csa) && (gvt->gd_csa == cs_addrs));
	/* keysize can be obtained from GVT->clue.top in case of GT.M.
	 * But for GT.CM client, clue will be uninitialized. So we would need to
	 * compute keysize from gv_cur_region->max_key_size. Since this is true for
	 * GT.M as well, we use the same approach for both to avoid an if check and a
	 * break in the pipeline.
	 */
	keysize = DBKEYSIZE(gv_cur_region->max_key_size);
	assert(!gvt->clue.top || (keysize == gvt->clue.top));
	lastsubslen = keysize - gvkey->prev - 2;
	assertpro((0 < lastsubslen) && (gvkey->top >= keysize) && (gvkey->end > gvkey->prev));
	ptr = &gvkey->base[gvkey->prev];
	memset(ptr, STR_SUB_MAXVAL, lastsubslen);
	ptr += lastsubslen;
	*ptr++ = KEY_DELIMITER;	 /* terminator for last subscript */
	*ptr = KEY_DELIMITER;    /* terminator for entire key */
	gvkey->end = gvkey->prev + lastsubslen + 1;
	assert(gvkey->end == (ptr - &gvkey->base[0]));
	if (NULL != gv_target->gd_csa)
		DBG_CHECK_GVTARGET_INTEGRITY(gvt);
}

static inline void gtcmtr_sub2str_xform_if_needed(gv_namehead *gvt, gv_key *gvkey, unsigned short key_top)
{
	unsigned char	*kprev, *kcur, *ktop;
	boolean_t	last_sub_is_null;

	if (gvt->collseq || gvt->nct)
	{	/* Need to convert subscript representation from client side to string representation
		 * so any collation transformations can happen on server side.
		 * First check if last subscript of incoming key is a NULL subscript.
		 * If so, client would have represented it using a sequence of FF, FF, FF, ...
		 * Remove the representation temporarily before doing the gv_xform_key.
		 * Introduce the NULL subscript after the transformation.
		 * This is because we do NOT allow a null subsc to be transformed to a non null subsc
		 * 	so no need for that be part of the transformation.
		 */
		last_sub_is_null = TRUE;
		kprev = &gvkey->base[gvkey->prev];
		for (kcur = kprev, ktop = &gvkey->base[key_top] - 1; kcur < ktop; kcur++)
		{
			if (STR_SUB_MAXVAL != *kcur)
			{
				last_sub_is_null = FALSE;
				break;
			}
		}
		if (last_sub_is_null)
		{
			*kprev = KEY_DELIMITER;	/* remove the null subscript temporarily */
			gvkey->end = gvkey->prev;
		}
		gv_xform_key(gvkey, FALSE);	/* do collation transform on server side */
		if (last_sub_is_null)
			gv_append_max_subs_key(gvkey, gvt); /* Insert the NULL subscript back */
	}
}

static inline void gv_undo_append_max_subs_key(gv_key *gvkey, gd_region *reg)
{
	assert(reg->std_null_coll || (STR_SUB_PREFIX == gvkey->base[gvkey->prev]));
	if (reg->std_null_coll)
		gvkey->base[gvkey->prev] = SUBSCRIPT_STDCOL_NULL;
	gvkey->base[gvkey->prev + 1] = KEY_DELIMITER;
	gvkey->end = gvkey->prev + 2;
	gvkey->base[gvkey->end] = KEY_DELIMITER;
}

static inline void dbg_check_gvtarget_gvcurrkey_in_sync(boolean_t check_csaddrs)
{
	mname_entry		*gvent;
	mstr			*varname;
	int			varlen;
	unsigned short		keyend;
	unsigned char		*keybase;

	GBLREF int4		gv_keysize;

	GBLREF gv_key		*gv_currkey;
	GBLREF gv_namehead	*reset_gv_target;

	assert((NULL != gv_currkey) || (NULL == gv_target));
	/* Make sure gv_currkey->top always reflects the maximum keysize across all dbs that we opened until now */
	assert((NULL == gv_currkey) || (gv_currkey->top == gv_keysize));
	if (!process_exiting)
	{
		keybase = &gv_currkey->base[0];
		if ((NULL != gv_currkey) && (0 != keybase[0]) && (0 != gv_currkey->end)
				&& (INVALID_GV_TARGET == reset_gv_target))
		{
			assert(NULL != gv_target);
			gvent = &gv_target->gvname;
			varname = &gvent->var_name;
			varlen = varname->len;
			assert(varlen);
			assert((0 != keybase[varlen]) || !memcmp(keybase, varname->addr, varlen));
			keyend = gv_currkey->end;
			assert(!keyend || (KEY_DELIMITER == keybase[keyend]));
			assert(!keyend || (KEY_DELIMITER == keybase[keyend - 1]));
			/* Check that gv_target is part of the gv_target_list */
			DBG_CHECK_GVT_IN_GVTARGETLIST(gv_target);
			if (check_csaddrs)
				DBG_CHECK_GVTARGET_CSADDRS_IN_SYNC;
		}
		/* Do gv_target sanity check too; Do not do this if it is NULL or if it is GT.CM GNP client (gd_csa is NULL) */
		if ((NULL != gv_target) && (NULL != gv_target->gd_csa))
			dbg_check_gvtarget_integrity(gv_target);
	}
}

#endif
