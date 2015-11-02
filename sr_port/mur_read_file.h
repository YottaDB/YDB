/****************************************************************
 *								*
 *	Copyright 2003, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef MUR_READ_FILE_H_INCLUDED
#define MUR_READ_FILE_H_INCLUDED

#ifdef UNIX
#  define MUR_BUFF_SIZE		(DISK_BLOCK_SIZE * 4096)
#else
#  define MUR_BUFF_SIZE		(DISK_BLOCK_SIZE * 256)
#endif

#if defined (MUR_USE_AIO) || defined(VMS)

#define MUR_FREAD_START(JCTL, BUFF_DESC, RET_STATUS)				\
{										\
	RET_STATUS = mur_fread_start(JCTL, BUFF_DESC);				\
}

#define MUR_FREAD_WAIT(JCTL, BUFF_DESC, RET_STATUS)				\
{										\
	RET_STATUS = mur_fread_wait(JCTL, BUFF_DESC);				\
}

#define MUR_FREAD_CANCEL(JCTL, MUR_DESC, RET_STATUS)				\
{										\
	RET_STATUS = mur_fread_cancel(JCTL);					\
}

#else /* !MUR_USE_AIO && !VMS */

#define MUR_FREAD_START(JCTL, BUFF_DESC, RET_STATUS)					\
{											\
	assert(JCTL->eof_addr > (BUFF_DESC)->dskaddr);					\
	(BUFF_DESC)->blen = MIN(MUR_BUFF_SIZE, JCTL->eof_addr - (BUFF_DESC)->dskaddr);	\
	DO_FILE_READ(JCTL->channel, (BUFF_DESC)->dskaddr, (BUFF_DESC)->base, 		\
			(BUFF_DESC)->blen, JCTL->status, JCTL->status2);		\
	(BUFF_DESC)->read_in_progress = TRUE;						\
	RET_STATUS = JCTL->status;							\
}

#define MUR_FREAD_WAIT(JCTL, BUFF_DESC, RET_STATUS)				\
{										\
	assert((BUFF_DESC)->read_in_progress);					\
	(BUFF_DESC)->read_in_progress = FALSE;					\
	RET_STATUS = SS_NORMAL;							\
}

#define MUR_FREAD_CANCEL(JCTL, MUR_DESC, RET_STATUS)				\
{										\
	MUR_DESC->seq_buff[0].read_in_progress = FALSE;				\
	MUR_DESC->seq_buff[1].read_in_progress = FALSE;				\
	RET_STATUS = SS_NORMAL;							\
}

#endif /* MUR_USE_AIO */

uint4	mur_fread_eof(jnl_ctl_list *jctl, reg_ctl_list *rctl);
uint4	mur_fread_eof_crash(jnl_ctl_list *jctl, off_jnl_t lo_off, off_jnl_t hi_off);
uint4 	mur_valrec_prev(jnl_ctl_list *jctl, off_jnl_t lo_off, off_jnl_t hi_off);
uint4 	mur_valrec_next(jnl_ctl_list *jctl, off_jnl_t offset);
/* Followings are not portable */
uint4 	mur_fread_start(jnl_ctl_list *jctl, mur_buff_desc_t *buff);
uint4 	mur_freadw(jnl_ctl_list *jctl, mur_buff_desc_t *buff);
uint4 	mur_fread_wait(jnl_ctl_list *jctl, mur_buff_desc_t *buff);
uint4 	mur_fread_cancel(jnl_ctl_list *jctl);

#endif /* MUR_READ_FILE_H_INCLUDED */
