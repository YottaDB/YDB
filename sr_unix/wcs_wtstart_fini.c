/****************************************************************
 *								*
 * Copyright (c) 2016-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

#include <error.h>

#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "wcs_sleep.h"
#include "filestruct.h"
#include "gtmio.h"
#include "wcs_wt.h"
#include "jnl.h"
#include "sleep_cnt.h"

#define SMALLNUMCRS	128 /* if a small number is requested use a local rather than the heap */

/* This function waits for "nbuffs" dirty buffers to be moved to the free queue (from the active or wip
 * queue) since function entry. If the active and wip queue become empty, it returns even if the "nbuffs"
 * target is not reached. It uses "cnl->wcs_buffs_freed" to help with this determination. Note that
 * "wcs_buffs_freed" can only be updated by "wcs_wtfini" (which holds crit) so we can expect to see a
 * monotonically increasing value of this shared counter.
 */

int wcs_wtstart_fini(gd_region *reg, int nbuffs, cache_rec_ptr_t cr2flush)
{
	int			numwritesneeded, numwritesissued;
	wtstart_cr_list_t	cr_list;
	cache_que_head_ptr_t 	crq, crwipq;
	cache_rec_ptr_t 	*listcrs, smalllistcrs[SMALLNUMCRS], *heaplistcrs;
	boolean_t		was_crit, wtstartalreadycalled = FALSE, proc_check, tried_to_flush = FALSE;
	gtm_uint64_t 		targetfreedlevel, basefreedlevel, prewtfinifreedlevel, lcl_buffs_freed;
	int4			err_status = 0, num_sleeps = 0;
	sgmnt_addrs             *csa;
	node_local_ptr_t	cnl;
	sgmnt_data_ptr_t	csd;
	jnl_private_control     *jpc;
	cache_rec_ptr_t		older_twin, cr2use;
#ifdef DEBUG
	gtm_uint64_t		dbg_wcs_buffs_freed, prev_dbg_wcs_buffs_freed = 0;
#endif

	csa = &FILE_INFO(reg)->s_addrs;
	csd = csa->hdr;
	cnl = csa->nl;
	crwipq = &csa->acc_meth.bg.cache_state->cacheq_wip;
	crq = &csa->acc_meth.bg.cache_state->cacheq_active;
	heaplistcrs = NULL;
	if (0 == nbuffs) /* if number not specified, use default (same as "wcs_wtstart") */
		nbuffs = csd->n_wrt_per_flu;
	basefreedlevel = cnl->wcs_buffs_freed;
        /* If we are asked to flush a specific cr AND it is the younger twin, check if the older twin needs
	 * to have its io initiated and reaped first.
	 */
	older_twin = (cache_rec_ptr_t) NULL;
	assert((NULL == cr2flush) || csa->now_crit);
	assert(csd->asyncio);
	if (cr2flush && cr2flush->twin && cr2flush->bt_index)
	{
		older_twin = (cache_rec_ptr_t) GDS_ANY_REL2ABS(csa, cr2flush->twin);
		if (older_twin->dirty)
			nbuffs++; /* We need to flush the older sibling too. */
	}
	targetfreedlevel = basefreedlevel + nbuffs;
	cr_list.listsize = nbuffs;
	if (SMALLNUMCRS < nbuffs)
	{
		heaplistcrs = (cache_rec_ptr_t *)malloc(SIZEOF(cache_rec_ptr_t) * nbuffs);
		cr_list.listcrs = heaplistcrs;
	} else /* if small number of buffers use the local */
		cr_list.listcrs = smalllistcrs;

	while (((lcl_buffs_freed = cnl->wcs_buffs_freed) < targetfreedlevel)
			&& (crq->fl || crwipq->fl))
	{ /* while we have not reached our free target and there are still things to free */
#		ifdef DEBUG
		dbg_wcs_buffs_freed = cnl->wcs_buffs_freed;
		assert(dbg_wcs_buffs_freed >= prev_dbg_wcs_buffs_freed);
		prev_dbg_wcs_buffs_freed = dbg_wcs_buffs_freed;
#		endif
		numwritesissued = 0;
		numwritesneeded = targetfreedlevel - lcl_buffs_freed;
		assert(numwritesneeded <= nbuffs);
		while (0 < numwritesneeded)
		{ /* issue the requested number of writes unless you run out of dirty buffers that don't have outstanding writes  */
#			ifdef DEBUG
			dbg_wcs_buffs_freed = cnl->wcs_buffs_freed;
			assert(dbg_wcs_buffs_freed >= prev_dbg_wcs_buffs_freed);
			prev_dbg_wcs_buffs_freed = dbg_wcs_buffs_freed;
#			endif
			assert(numwritesneeded <= nbuffs);
			/* wcs_wtstart() will initiate up to numwritesneeded writes (limited by the number of dirty buffers
			 * available and buffers being blocked from having their writes initiated)
			 */
			/* If we are asked to flush a specific cr AND it is the younger twin, check if the older twin needs
			 * to have its io initiated (via wcs_wtstart()) (possible if its write errored out and it transitioned
			 * from the wip queue back to the active queue). If it does call wcs_wtstart with the older twin as it
			 * must be flushed first before we can attempt to flush the newer twin.
			 */
			if (older_twin)
		 	{
				if (older_twin->dirty && !older_twin->epid)
				{
					wtstartalreadycalled = TRUE; /* this variable indicates if we ever called wcs_wtstart */
					tried_to_flush = TRUE; /* this variable indicates if we called it in this iteration */
					err_status = wcs_wtstart(reg, numwritesneeded, &cr_list, older_twin);
				} else
				{
					tried_to_flush = FALSE; /* remember that we did not try to initiate any ios */
					cr_list.numcrs = 0; /* since wcs_wtstart was not called indicate 0 ios were initiated. */
					assert(!err_status); /* we should only get here if err_status is 0 */
					err_status = 0; /* for PRO ensure it is set to 0 */
				}

			} else
			{
				wtstartalreadycalled = TRUE;
				tried_to_flush = TRUE;
				err_status = wcs_wtstart(reg, numwritesneeded, &cr_list, cr2flush);
			}
			if (err_status || cnl->wc_blocked) /* if we are blocked get out to allow cache recovery */
				break;
			if (0 != cr_list.numcrs)
			{ /* If we were able to initiate some writes wait for their statuses to complete */
				wcs_wtfini_nocrit(reg, &cr_list);
				numwritesissued += cr_list.numcrs;
				num_sleeps = 0; /* making progress so reset the clock */
				if (numwritesissued >= numwritesneeded)
					break; /* enough i/o statuses are back so lets reap them */
			} else
			 	/* not able to initiate any i/os:
				 * maybe out of i/o slots, buffer(s) may be stuck (jnl write, twin, bt_put)
				 * or we are out of dirty buffers to initiate writes on but there are crs
				 * in the WIP queue or we have previously issued the io for an older twin
				 * and now need to wait for the io to complete (or tried_to_flush=FALSE case).
				 */
				break;
			/* we waited in wcs_wtfini_nocrit() above so check cnl->wcs_buffs_freed to see if any other processes
			 *  helped increase the number of freed buffers.
			 */
			numwritesneeded = targetfreedlevel - cnl->wcs_buffs_freed;
		}
		/* if we got an error from wcs_wtstart() or wcs_wtfini() or we were asked to flush a specific buffer
		 * and it is no longer dirty or cache needs to be verified we are done.
		 */
		if (err_status || (cr2flush && !cr2flush->dirty) || cnl->wc_blocked)
			break;
		/* have i/o statuses on i/os we issued so call wcs_wtfini() to reap them all at once;
		 * other's i/os may also be reaped
		 */
		prewtfinifreedlevel = cnl->wcs_buffs_freed; /* get a base line to see if wtfini is making progress */
		was_crit = csa->now_crit;
		if (!was_crit)
			grab_crit(reg);
		/* Reap our i/os (and maybe some others).
		 * If we tried to flush, were unable to, and there is something on the wip queue: a process with an outstanding
		 * i/o may have died and is blocking our ability to initiate i/os; ask wcs_wtfini() to see if all of the processes
		 * that have outstanding writes are alive.
		 */
		proc_check = tried_to_flush && (0 == cr_list.numcrs) && crwipq->fl
			? CHECK_IS_PROC_ALIVE_TRUE : CHECK_IS_PROC_ALIVE_FALSE;
		/* If we are waiting on the completion of the io of an older twin use its cr on the call to wcs_wtfini */
		cr2use = (older_twin && older_twin->dirty & older_twin->epid) ? older_twin : cr2flush;
		err_status = wcs_wtfini(reg, proc_check, cr2use);
                if (older_twin && !older_twin->dirty)
                        /* the older twin has been flushed so we are done with it. */
                        older_twin = (cache_rec_ptr_t) NULL;
		if (!was_crit)
			rel_crit(reg);
		if (prewtfinifreedlevel == cnl->wcs_buffs_freed)
		{
			wcs_sleep(1);	/* wtfini did not make any progress so wait a while.
					 * Note: The sleep time of 1 msec here is factored into the calculation
					 * of MAX_WTSTART_FINI_SLEEPS (it has a MAXSLPTIME multiplicative factor).
					 * Any changes to the 1 parameter need corresponding changes in the macro.
					 */
			num_sleeps++;
		} else
			num_sleeps = 0; /* we are making progress so reset the clock */
		if (MAX_WTSTART_FINI_SLEEPS < num_sleeps) /* waited long enough; initiate cache recovery */
		{
			SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
			break;
		}
		if (err_status)
			break;
	}
	if (!wtstartalreadycalled)
	{
        	jpc = csa->jnl;
        	assert(!JNL_ALLOWED(csd) || ( NULL != jpc));     /* if journaling is allowed, we better have non-null csa->jnl */
		/* wcs_wtstart(), which would call jnl_qio_start(), has not been called. Call jnl_qio_start() in case there are
		 * journal buffers that need to be flushed.
		 */
        	if (JNL_ENABLED(csd) && (NULL != jpc) && (NOJNL != jpc->channel))
                	jnl_qio_start(jpc);
	}
	if (NULL != heaplistcrs)
		free(heaplistcrs);
	return err_status;
}
