/****************************************************************
 *								*
 *	Copyright 2003, 2005 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef MUR_READ_FILE_H_INCLUDED
#define MUR_READ_FILE_H_INCLUDED

#define MUR_BUFF_SIZE (DISK_BLOCK_SIZE * 256)

#if defined (MUR_USE_AIO) || defined(VMS)

#define MUR_FREAD_START(BUFF_DESC, RET_STATUS)					\
{										\
	RET_STATUS = mur_fread_start(BUFF_DESC);				\
}

#define MUR_FREAD_WAIT(BUFF_DESC, RET_STATUS)					\
{										\
	RET_STATUS = mur_fread_wait(BUFF_DESC);					\
}

#define MUR_FREAD_CANCEL(RET_STATUS)						\
{										\
	RET_STATUS = mur_fread_cancel();					\
}

#else /* !MUR_USE_AIO && !VMS */

#define MUR_FREAD_START(BUFF_DESC, RET_STATUS)							\
{												\
	assert(mur_jctl->eof_addr > (BUFF_DESC)->dskaddr);					\
	(BUFF_DESC)->blen = MIN(MUR_BUFF_SIZE, mur_jctl->eof_addr - (BUFF_DESC)->dskaddr);	\
	DO_FILE_READ(mur_jctl->channel, (BUFF_DESC)->dskaddr, (BUFF_DESC)->base, 		\
			(BUFF_DESC)->blen, mur_jctl->status, mur_jctl->status2);		\
	(BUFF_DESC)->read_in_progress = TRUE;							\
	RET_STATUS = mur_jctl->status;								\
}

#define MUR_FREAD_WAIT(BUFF_DESC, RET_STATUS)					\
{										\
	assert((BUFF_DESC)->read_in_progress);					\
	(BUFF_DESC)->read_in_progress = FALSE;					\
	RET_STATUS = SS_NORMAL;							\
}

#define MUR_FREAD_CANCEL(RET_STATUS)						\
{										\
	mur_desc.seq_buff[0].read_in_progress = FALSE;				\
	mur_desc.seq_buff[1].read_in_progress = FALSE;				\
	RET_STATUS = SS_NORMAL;							\
}

#endif /* MUR_USE_AIO */

typedef struct
{
	unsigned char		*base;		/* Pointer to the buffer base of this mur_buff_desc */
	unsigned char		*top;		/* Pointer to the buffer top of this mur_buff_desc */
	int4			blen;		/* Length of the buffer till end of valid data  */
	off_jnl_t		dskaddr;   	/* disk offset from which this buffer was read */
	boolean_t		read_in_progress;/* Asynchronous read requested and in progress */
#if defined(UNIX)
	struct aiocb 		*aiocbp;
	int			rip_channel;	/* channel that has the aio read (for this mur_buff_desc_t) in progress.
						 * valid only if "read_in_progress" field is TRUE.
						 * this is a copy of the active channel "mur_jctl->channel" while issuing the AIO.
						 * in case the active channel "mur_jctl->channel" changes later (due to switching
						 * to a different journal file) and we want to cancel the previously issued aio
						 * we cannot use mur_jctl->channel but should use "rip_channel" for the cancel.
						 */
#elif defined(VMS)
	io_status_block_disk	iosb;
	short			rip_channel;	/* same meaning as the Unix field */
	short			filler;		/* to ensure 4-byte alignment for this structure */
#endif
} mur_buff_desc_t;

typedef struct
{
	int4			blocksize;	/* This amount it reads from disk to memory */
	unsigned char		*alloc_base;	/* Pointer to the buffers allocated. All 3 buffer allocated at once */
	int4			alloc_len;	/* Size of alloc_base buffer */
	mur_buff_desc_t		random_buff;	/* For reading pini_rec which could be at a random offset before current record */
	unsigned char		*aux_buff1;	/* For partial records for mur_next at the end of seq_buff[1] */
	mur_buff_desc_t		seq_buff[2];	/* Two buffers for double buffering  */
	mur_buff_desc_t		aux_buff2;	/* For partial records for mur_prev just previous of seq_buff[0] or for overflow */
	int			index;		/* Which one of the two seq_buff is in use */
	mur_buff_desc_t		*cur_buff;	/* pointer to active mur_buff_desc_t */
	mur_buff_desc_t		*sec_buff; 	/* pointer to second mur_buff_desc_t for the double buffering*/
} mur_read_desc_t;

uint4	mur_fread_eof (jnl_ctl_list *jctl);
uint4	mur_fread_eof_crash(jnl_ctl_list *jctl, off_jnl_t lo_off, off_jnl_t hi_off);
uint4 	mur_valrec_prev(jnl_ctl_list *jctl, off_jnl_t lo_off, off_jnl_t hi_off);
uint4 	mur_valrec_next(jnl_ctl_list *jctl, off_jnl_t offset);
/* Followings are not portable */
uint4 	mur_fread_start(mur_buff_desc_t *buff);
uint4 	mur_freadw (mur_buff_desc_t *buff);
uint4 	mur_fread_wait(mur_buff_desc_t *buff);
uint4 	mur_fread_cancel(void);

#endif /* MUR_READ_FILE_H_INCLUDED */
