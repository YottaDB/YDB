/****************************************************************
 *								*
 *	Copyright 2003, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#if defined(UNIX)
#include "gtm_aio.h"
#elif defined(VMS)
#include "iosb_disk.h"
#include "iosp.h"	/* for SS_NORMAL */
#endif
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "hashtab.h"
#include "muprec.h"
#include "mur_read_file.h"

GBLREF 	jnl_gbls_t	jgbl;
GBLREF	mur_gbls_t	murgbl;
GBLREF	void		(*call_on_signal)();
GBLREF	unsigned char	t_fail_hist[CDB_MAX_TRIES];
GBLREF	boolean_t	gv_play_duplicate_kills;

#define	FREE_AND_NULLIFY_BUDDY_LIST(LIST)	\
{						\
	assert(NULL != LIST);			\
	if (NULL != LIST)			\
	{					\
		FREEUP_BUDDY_LIST(LIST);	\
		LIST = NULL;			\
	}					\
}

#define	FREE_AND_NULLIFY(PTR)		\
{					\
	assert(NULL != PTR);		\
	if (NULL != PTR)		\
	{				\
		free(PTR);		\
		PTR = NULL;		\
	}				\
}

/****************************************************************************************
 * Function Name: mur_init()							       	*
 * Input: None										*
 * Output: None										*
 ****************************************************************************************/

void mur_init(void)
{
	int	index;

	murgbl.multi_list = (buddy_list *)malloc(SIZEOF(buddy_list));
	initialize_list(murgbl.multi_list, SIZEOF(multi_struct), MUR_MULTI_LIST_INIT_ALLOC);
	murgbl.forw_multi_list = (buddy_list *)malloc(SIZEOF(buddy_list));
	initialize_list(murgbl.forw_multi_list, SIZEOF(forw_multi_struct), MUR_MULTI_LIST_INIT_ALLOC);
	init_hashtab_int8(&murgbl.token_table, MUR_MULTI_HASHTABLE_INIT_ELEMS, HASHTAB_COMPACT, HASHTAB_SPARE_TABLE);
	init_hashtab_int8(&murgbl.forw_token_table, MUR_MULTI_HASHTABLE_INIT_ELEMS, HASHTAB_COMPACT, HASHTAB_SPARE_TABLE);
	murgbl.pini_buddy_list = (buddy_list *)malloc(SIZEOF(buddy_list));
	initialize_list(murgbl.pini_buddy_list, SIZEOF(pini_list_struct), MUR_PINI_LIST_INIT_ELEMS);
	/* pini_list hash table of a jnl_ctl_list is initialized in mur_fopen */
	/* FAB of a jnl_ctl_list is initialized in mur_fopen */
	murgbl.resync_seqno = 0; 		/* interrupted rollback set this to non-zero value later */
	jgbl.mupip_journal = TRUE;	/* this is a MUPIP JOURNAL command */
	/* Write journal records for KILL even if node does not exist in this database. This way if any process had
	 * written a KILL record for a no-op kill (a kill of a node that does not exist; this is currently possible in
	 * the update process), recovery does the exact same thing and thereby keeps the db/jnl in sync.
	 */
	gv_play_duplicate_kills = TRUE;
	/* Because journal recovery operates with no one else touching the database, t_tries is going to be
	 * directly set to CDB_STAGNATE for every TP transaction (in op_tstart). So initialize
	 * t_fail_hist[0] through t_fail_hist[CDB_STAGNATE-1] to '0' for display purposes.
	 */
	for (index = 0; index < CDB_STAGNATE; index++)
		t_fail_hist[index] = '0';
	DEBUG_ONLY(assert_jrec_member_offsets();)
}

void	mur_free(void)
{
	FREE_AND_NULLIFY_BUDDY_LIST(murgbl.multi_list);
	FREE_AND_NULLIFY_BUDDY_LIST(murgbl.forw_multi_list);
	FREE_AND_NULLIFY_BUDDY_LIST(murgbl.pini_buddy_list);
	/* Note : In addition, there is a lot of mallocs done in mur_open_files (of rctls and jctls) that need to be freed.
	 * It is not considered as important since MUPIP is a one-command image activation so no memory leak issues.
	 */
}

void	mur_rctl_desc_alloc(reg_ctl_list *rctl)
{
	mur_read_desc_t			*mur_desc;

	mur_desc = (mur_read_desc_t *)malloc(SIZEOF(mur_read_desc_t));
	/*
	 * Layout of rctl->mur_desc buffers (consecutive buffers are contiguous)
	 *
	 * |<--MUR_BUFF_SIZE-->|<--MUR_BUFF_SIZE-->|<--MUR_BUFF_SIZE-->|<--MUR_BUFF_SIZE-->|<--MUR_BUFF_SIZE-->|
	 * |   random_buff     |     aux_buff1     |    seq_buff[0]    |    seq_buff[1]    |     aux_buff2     |
	 */
	assert(MUR_BUFF_SIZE > MAX_JNL_REC_SIZE);	/* in order for a journal record to fit in one buffer */
	mur_desc->alloc_base = malloc(5 * MUR_BUFF_SIZE); /* two for double buffering, two for auxiliary, one for random */
	mur_desc->random_buff.base = mur_desc->alloc_base;
	mur_desc->random_buff.top  = mur_desc->aux_buff1 = mur_desc->alloc_base + MUR_BUFF_SIZE;
	mur_desc->seq_buff[0].base = mur_desc->aux_buff1 + MUR_BUFF_SIZE;
	mur_desc->seq_buff[0].top = mur_desc->seq_buff[1].base = mur_desc->seq_buff[0].base + MUR_BUFF_SIZE;
	mur_desc->seq_buff[1].top = mur_desc->aux_buff2.base = mur_desc->seq_buff[1].base + MUR_BUFF_SIZE;
	mur_desc->aux_buff2.top = mur_desc->seq_buff[1].top + MUR_BUFF_SIZE;
	mur_desc->random_buff.read_in_progress = FALSE;
	mur_desc->seq_buff[0].read_in_progress = FALSE;
	mur_desc->seq_buff[1].read_in_progress = FALSE;
	mur_desc->aux_buff2.read_in_progress = FALSE;
#	if defined(UNIX) && defined(MUR_USE_AIO)
	mur_desc->seq_buff[0].aiocbp = (struct aiocb *)malloc(SIZEOF(struct aiocb));
	mur_desc->seq_buff[1].aiocbp = (struct aiocb *)malloc(SIZEOF(struct aiocb));
	memset((char *)mur_desc->seq_buff[0].aiocbp, 0, SIZEOF(struct aiocb));
	memset((char *)mur_desc->seq_buff[1].aiocbp, 0, SIZEOF(struct aiocb));
	mur_desc->aux_buff2.aiocbp = (struct aiocb *)NULL;	/* no aio for this buffer */
	mur_desc->random_buff.aiocbp = (struct aiocb *)NULL;	/* no aio for this buffer */
#	elif defined(VMS)
	mur_desc->random_buff.iosb.cond = SS_NORMAL;
	mur_desc->seq_buff[0].iosb.cond = SS_NORMAL;
	mur_desc->seq_buff[1].iosb.cond = SS_NORMAL;
#	endif
	rctl->mur_desc = mur_desc;
}

void	mur_rctl_desc_free(reg_ctl_list *rctl)
{
	int			index;
	mur_read_desc_t		*mur_desc;

	mur_desc = rctl->mur_desc;
	FREE_AND_NULLIFY(mur_desc->alloc_base);
#	if defined(UNIX) && defined(MUR_USE_AIO)
	for (index = 0; index <= 1; index++)
		FREE_AND_NULLIFY(mur_desc->seq_buff[index].aiocbp);
#	endif
	FREE_AND_NULLIFY(rctl->mur_desc); /* cannot use mur_desc since we want rctl->mur_desc (not mur_desc) to be NULLified */
}
