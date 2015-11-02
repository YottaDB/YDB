/****************************************************************
 *								*
 *	Copyright 2003, 2009 Fidelity Information Services, Inc	*
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
#include "muprec.h"
#include "mur_read_file.h"

GBLREF 	jnl_gbls_t	jgbl;
GBLREF	mur_read_desc_t	mur_desc;
GBLREF	mur_gbls_t	murgbl;
GBLREF	boolean_t	is_standalone;
GBLREF	boolean_t	gvdupsetnoop; /* if TRUE, duplicate SETs update journal but not database (except for curr_tn++) */
GBLREF	void		(*call_on_signal)();
GBLREF	unsigned char	t_fail_hist[CDB_MAX_TRIES];


/****************************************************************************************
 * Function Name: mur_init()							       	*
 * Input: None										*
 * Output: None										*
 *											*
 * *************************************************************************************/

void mur_init(void)
{
	int	index;

	/*
	 * Layout of mur_desc buffers (consecutive buffers are contiguous)
	 *
	 * |<--MUR_BUFF_SIZE-->|<--MUR_BUFF_SIZE-->|<--MUR_BUFF_SIZE-->|<--MUR_BUFF_SIZE-->|<--MUR_BUFF_SIZE-->|
	 * |   random_buff     |     aux_buff1     |    seq_buff[0]    |    seq_buff[1]    |     aux_buff2     |
	 */
	assert(MUR_BUFF_SIZE > MAX_JNL_REC_SIZE);	/* in order for a journal record to fit in one buffer */
	mur_desc.alloc_base = malloc(5 * MUR_BUFF_SIZE); /* two for double buffering, two for auxiliary, one for random */
	mur_desc.random_buff.base = mur_desc.alloc_base;
	mur_desc.random_buff.top  = mur_desc.aux_buff1 = mur_desc.alloc_base + MUR_BUFF_SIZE;
	mur_desc.seq_buff[0].base = mur_desc.aux_buff1 + MUR_BUFF_SIZE;
	mur_desc.seq_buff[0].top = mur_desc.seq_buff[1].base = mur_desc.seq_buff[0].base + MUR_BUFF_SIZE;
	mur_desc.seq_buff[1].top = mur_desc.aux_buff2.base = mur_desc.seq_buff[1].base + MUR_BUFF_SIZE;
	mur_desc.aux_buff2.top = mur_desc.seq_buff[1].top + MUR_BUFF_SIZE;
	mur_desc.random_buff.read_in_progress = FALSE;
	mur_desc.seq_buff[0].read_in_progress = FALSE;
	mur_desc.seq_buff[1].read_in_progress = FALSE;
#	if defined(UNIX) && defined(MUR_USE_AIO)
	mur_desc.seq_buff[0].aiocbp = (struct aiocb *)malloc(sizeof(struct aiocb));
	mur_desc.seq_buff[1].aiocbp = (struct aiocb *)malloc(sizeof(struct aiocb));
	memset((char *)mur_desc.seq_buff[0].aiocbp, 0, sizeof(struct aiocb));
	memset((char *)mur_desc.seq_buff[1].aiocbp, 0, sizeof(struct aiocb));
	mur_desc.aux_buff2.aiocbp = (struct aiocb *)NULL;	/* no aio for this buffer */
	mur_desc.random_buff.aiocbp = (struct aiocb *)NULL;	/* no aio for this buffer */
#	elif defined(VMS)
	mur_desc.random_buff.iosb.cond = SS_NORMAL;
	mur_desc.seq_buff[0].iosb.cond = SS_NORMAL;
	mur_desc.seq_buff[1].iosb.cond = SS_NORMAL;
#	endif
	murgbl.multi_list = (buddy_list *)malloc(sizeof(buddy_list));
	initialize_list(murgbl.multi_list, sizeof(multi_struct), MUR_MULTI_LIST_INIT_ALLOC);
	init_hashtab_int8(&murgbl.token_table, MUR_MULTI_HASHTABLE_INIT_ELEMS);
	murgbl.pini_buddy_list = (buddy_list *)malloc(sizeof(buddy_list));
	initialize_list(murgbl.pini_buddy_list, sizeof(pini_list_struct), MUR_PINI_LIST_INIT_ELEMS);
	/* pini_list hash table of a jnl_ctl_list is initialized in mur_fopen */
	/* FAB of a jnl_ctl_list is initialized in mur_fopen */
	murgbl.resync_seqno = 0; 		/* interrupted rollback set this to non-zero value later */
	murgbl.stop_rlbk_seqno = MAXUINT8;	/* allow default rollback to continue forward processing till last valid record */
	jgbl.mupip_journal = TRUE;	/* this is a MUPIP JOURNAL command */
	murgbl.db_updated = FALSE;
	gvdupsetnoop = FALSE; /* disable optimization to avoid multiple updates to the database and journal for duplicate sets */
	/* this is because, like the update process, MUPIP JOURNAL RECOVER/ROLLBACK is supposed to simulate GTM
	 * update activity else there will be transaction-number mismatch in the database */
	is_standalone = TRUE;
	/* Because is_standalone is TRUE, t_tries is going to be directly set to CDB_STAGNATE for every TP transaction.
	 * So initialize t_fail_hist[0] to t_fail_hist[CDB_STAGNATE-1] to '0' for display purposes.
	 */
	for (index = 0; index < CDB_STAGNATE; index++)
		t_fail_hist[index] = '0';
	call_on_signal = mur_close_files;
	DEBUG_ONLY(assert_jrec_member_offsets();)
}
