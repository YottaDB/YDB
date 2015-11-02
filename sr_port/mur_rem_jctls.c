/****************************************************************
 *								*
 *	Copyright 2003, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "gdsroot.h"
#include "gdsblk.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"
#include "iosp.h"
#include "jnl_typedef.h"
#include "gtmmsg.h"
#include "gtm_file_remove.h"

GBLREF 	mur_gbls_t	murgbl;

void	mur_rem_jctls(reg_ctl_list *rctl)
{	/* free all structures from generations that follow the one that has the turnaround point through latest generation */
	jnl_ctl_list	*jctl, *earliest2close, *free_this;
	int4		status;
	uint4		ustatus;
	boolean_t	rem_jctls_done;
	error_def	(ERR_FILEDELFAIL);
	error_def	(ERR_FILEDEL);

	earliest2close = jctl = rctl->jctl_alt_head;
	assert(NULL != jctl);
	assert((NULL != jctl->prev_gen && jctl->prev_gen->next_gen != jctl));
	/* since we maintain only previous gener links in the journal file header,
	 * to avoid loss of links, we traverse the journal gener list in reverse order */
	for ( ; NULL != jctl->next_gen; jctl = jctl->next_gen) /* find the latest gener */
		;
	do
	{
		rem_jctls_done = (jctl == earliest2close);
		assert((rctl->csd->jnl_file_len != jctl->jnl_fn_len)
			|| (0 != memcmp(rctl->csd->jnl_file_name, jctl->jnl_fn, jctl->jnl_fn_len)));
		if (!jctl->jfh->recover_interrupted)
			GTMASSERT; /* Out of design situation */
		assert(jctl->reg_ctl == rctl);
		if (!mur_fclose(jctl))
			murgbl.wrn_count++;	/* mur_fclose() would have done the appropriate gtm_putmsg() */
		if (SS_NORMAL != (status = gtm_file_remove((char *)jctl->jnl_fn, jctl->jnl_fn_len, &ustatus)))
		{
			murgbl.wrn_count++;
			gtm_putmsg(VARLSTCNT1 (6) ERR_FILEDELFAIL, 2, jctl->jnl_fn_len, jctl->jnl_fn,
				status, PUT_SYS_ERRNO(ustatus));
		} else
			gtm_putmsg(VARLSTCNT (4) ERR_FILEDEL, 2, jctl->jnl_fn_len, jctl->jnl_fn);
		/* It is possible (in case of repeated interrupted recoveries with differing turn-around points) that
		 * jctl->prev_gen->next_gen is not "jctl" but instead NULL or a different (yet valid) jctl entry (asserted below).
		 * Therefore, in order to free "jctl" and yet be able to move to its prev_gen, we note it down in a
		 * local variable for doing the free. It is exactly for the above reason that we should not reset
		 * jctl->next_gen to NULL after the free.
		 */
		free_this = jctl;
		jctl = jctl->prev_gen;
		assert(!rem_jctls_done || (jctl->next_gen != free_this));
		free(free_this);
	} while (!rem_jctls_done);
}
