/****************************************************************
 *								*
 *	Copyright 2003 Sanchez Computer Associates, Inc.	*
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
#include "hashdef.h"
#include "buddy_list.h"
#include "muprec.h"
#include "mur_read_file.h"

GBLREF	mur_read_desc_t	mur_desc;
GBLREF	mur_gbls_t	murgbl;


/****************************************************************************************
 * Function Name: mur_init()							       	*
 * Input: None										*
 * Output: None										*
 *											*
 * *************************************************************************************/

void mur_init(void)
{
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
#if defined(UNIX)
	mur_desc.seq_buff[0].aiocbp = (struct aiocb *)malloc(sizeof(struct aiocb));
	mur_desc.seq_buff[1].aiocbp = (struct aiocb *)malloc(sizeof(struct aiocb));
	memset((char *)mur_desc.seq_buff[0].aiocbp, 0, sizeof(struct aiocb));
	memset((char *)mur_desc.seq_buff[1].aiocbp, 0, sizeof(struct aiocb));
	mur_desc.aux_buff2.aiocbp = (struct aiocb *)NULL;	/* no aio for this buffer */
	mur_desc.random_buff.aiocbp = (struct aiocb *)NULL;	/* no aio for this buffer */
#elif defined(VMS)
	mur_desc.random_buff.iosb.cond = SS_NORMAL;
	mur_desc.seq_buff[0].iosb.cond = SS_NORMAL;
	mur_desc.seq_buff[1].iosb.cond = SS_NORMAL;
#endif
	murgbl.multi_list = (buddy_list *)malloc(sizeof(buddy_list));
	initialize_list(murgbl.multi_list, sizeof(multi_struct), MUR_MULTI_LIST_INIT_ALLOC);
	ht_init(&murgbl.token_table, MUR_MULTI_HASHTABLE_INIT_ELEMS);
	murgbl.pini_buddy_list = (buddy_list *)malloc(sizeof(buddy_list));
	initialize_list(murgbl.pini_buddy_list, sizeof(pini_list_struct), MUR_PINI_LIST_INIT_ELEMS);
	/* pini_list hash table of a jnl_ctl_list is initialized in mur_fopen */
	/* FAB of a jnl_ctl_list is initialized in mur_fopen */
}
