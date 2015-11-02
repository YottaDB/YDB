/****************************************************************
 *								*
 *	Copyright 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stdio.h"

#include "gtmio.h"
#include "error.h"
#include "mv_stent.h"

GBLREF	unsigned char	*stackbase;

#ifdef DEBUG_CONDSTK
#  define DBGCSTK(x) DBGFPF(x)
#else
#  define DBGCSTK(x)
#endif

/* Expands the condition handler stack copying old stack to new expanded stack.
 *
 * Note, chnd_end is always set 2 entries from the actual true top of the stack. Consider what can happen
 * if we go to expand the stack but storage is not available. We will fail but when running things down,
 * we ALSO need to install handlers. So once process_exiting is set, we are allowed to use the extra handlers.
 */
void condstk_expand(void)
{
	condition_handler	*new_chnd, *new_chnd_end, *ctxt_ent;
	int			new_size, old_len, old_size, cnt;
	UINTPTR_T		delta;
	mv_stent		*mvs;

	DBGEHND((stderr, "condstk_expand: old: chnd: "lvaddr"  chnd_end: "lvaddr"  ctxt: "lvaddr"  active_ch: "lvaddr
		 "  chnd_incr: %d\n", chnd, chnd_end, ctxt, active_ch, chnd_incr));
	/* Make sure we are allowed to expand */
	old_len = INTCAST((char *)chnd_end - (char *)chnd);
	old_size = old_len / SIZEOF(condition_handler);
	new_size = old_size + chnd_incr + CONDSTK_RESERVE;		/* New count of entries in cond handlr stack */
	/* Nothing known should allow/require stack to get this large */
	assertpro(new_size <= CONDSTK_MAX_STACK);
	new_chnd = malloc(new_size * SIZEOF(condition_handler));	/* Allocate new condition handler stack */
	new_chnd_end = &new_chnd[new_size];
	delta = (UINTPTR_T)((char *)new_chnd - (char *)chnd);
	memcpy(new_chnd, chnd, old_len);				/* Copy occupied part of old stack */
	assert(chnd < chnd_end);
	/* Modify the address of save_active_ch so points to relevant entry in new condition handler stack. Note first
	 * entry back pointer remains unmodified (as NULL).
	 */
	for (cnt = 2, ctxt_ent = new_chnd + 1; cnt <= old_size; cnt++, ctxt_ent++)
	{
		assert(ctxt_ent >= new_chnd);
		assert(ctxt_ent < new_chnd_end);
		DBGEHND((stderr, "condstk_expand: cnt: %d, chptr: 0x"lvaddr"  save_active_ch from 0x"lvaddr
			 " to 0x"lvaddr"\n", cnt, ctxt_ent, ctxt_ent->save_active_ch, ((char *)ctxt_ent->save_active_ch + delta)));
		ctxt_ent->save_active_ch = (condition_handler *)((char *)ctxt_ent->save_active_ch + delta);
		assert((1 == cnt) || (ctxt_ent->save_active_ch >= new_chnd));
		assert((1 == cnt) || (ctxt_ent->save_active_ch < new_chnd_end));
	}
#	ifdef GTM_TRIGGER
	/* Trigger type mv_stent (MVST_TRIGR) save/restore the value of ctxt so look through the stack to locate those and
	 * fix them up too.
	 */
	for (mvs = mv_chain; mvs < (mv_stent *)stackbase; mvs = (mv_stent *)((char *)mvs + mvs->mv_st_next))
	{
		if (MVST_TRIGR != mvs->mv_st_type)
			continue;
		DBGEHND((stderr, "condstk_expand: Trigger saved ctxt modified from 0x"lvaddr" to 0x"lvaddr"\n",
			 mvs->mv_st_cont.mvs_trigr.ctxt_save, (char *)mvs->mv_st_cont.mvs_trigr.ctxt_save + delta));
		/* Have a trigger mv_stent - appropriately modify the saved ctxt value (high water mark for condition handlers */
		mvs->mv_st_cont.mvs_trigr.ctxt_save = (condition_handler *)((char *)mvs->mv_st_cont.mvs_trigr.ctxt_save + delta);
	}
#	endif
	/* Condition handler stack now reset - modify globals of record accordingly */
	free(chnd);		/* Old version no longer needed */
	chnd = new_chnd;
	chnd_end = new_chnd_end;
	if (CONDSTK_MAX_INCR > chnd_incr)
		chnd_incr = chnd_incr * 2;
	ctxt = (condition_handler *)((char *)ctxt + delta);
	active_ch = (condition_handler *)((char *)active_ch + delta);
	assert(ctxt >= chnd);
	assert(ctxt < chnd_end);
	assert(active_ch >= chnd);
	assert(active_ch < chnd_end);
	DBGEHND((stderr, "condstk_expand: new: chnd: "lvaddr"  chnd_end: "lvaddr"  ctxt: "lvaddr"  active_ch: "lvaddr
		 "  chnd_incr: %d  delta: "lvaddr"\n", chnd, chnd_end, ctxt, active_ch, chnd_incr, delta));
}
