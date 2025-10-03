/****************************************************************
 *								*
 * Copyright (c) 2003-2025 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

/* #GTM_THREAD_SAFE : The below macro (MUR_FREAD_START) is thread-safe */
#define MUR_FREAD_START(JCTL, BUFF_DESC, RET_STATUS)					\
{											\
	RET_STATUS = mur_fread_start(JCTL, BUFF_DESC);					\
}

/* #GTM_THREAD_SAFE : The below macro (MUR_FREAD_WAIT) is thread-safe */
#define MUR_FREAD_WAIT(JCTL, BUFF_DESC, RET_STATUS)					\
{											\
	RET_STATUS = mur_fread_wait(JCTL, BUFF_DESC);					\
}

/* #GTM_THREAD_SAFE : The below macro (MUR_FREAD_CANCEL) is thread-safe */
#define MUR_FREAD_CANCEL(JCTL, MUR_DESC, RET_STATUS)					\
{											\
	RET_STATUS = mur_fread_cancel(JCTL);						\
}

#else /* !MUR_USE_AIO && !VMS */

/* #GTM_THREAD_SAFE : The below macro (MUR_FREAD_START) is thread-safe */
#define MUR_FREAD_START(JCTL, BUFF_DESC, RET_STATUS)					\
{											\
	assert(JCTL->eof_addr > (BUFF_DESC)->dskaddr);					\
	(BUFF_DESC)->blen = MIN(MUR_BUFF_SIZE, JCTL->eof_addr - (BUFF_DESC)->dskaddr);	\
	DO_FILE_READ(JCTL->channel, (BUFF_DESC)->dskaddr, (BUFF_DESC)->base, 		\
			(BUFF_DESC)->blen, JCTL->status, JCTL->status2);		\
	(BUFF_DESC)->read_in_progress = TRUE;						\
	RET_STATUS = JCTL->status;							\
}

/* #GTM_THREAD_SAFE : The below macro (MUR_FREAD_WAIT) is thread-safe */
#define MUR_FREAD_WAIT(JCTL, BUFF_DESC, RET_STATUS)					\
{											\
	assert((BUFF_DESC)->read_in_progress);						\
	(BUFF_DESC)->read_in_progress = FALSE;						\
	RET_STATUS = SS_NORMAL;								\
}

/* #GTM_THREAD_SAFE : The below macro (MUR_FREAD_CANCEL) is thread-safe */
#define MUR_FREAD_CANCEL(JCTL, MUR_DESC, RET_STATUS)					\
{											\
	MUR_DESC->seq_buff[0].read_in_progress = FALSE;					\
	MUR_DESC->seq_buff[1].read_in_progress = FALSE;					\
	RET_STATUS = SS_NORMAL;								\
}

#endif /* MUR_USE_AIO */
#define COPY_JNL_PATH(JCTL, CPTR, CPTR_LAST, NEW_PATH)					\
MBSTART {										\
	memcpy((JCTL)->jnl_fn, (NEW_PATH)->buff, (NEW_PATH)->len);			\
	(JCTL)->jnl_fn[(NEW_PATH)->len] = '/';						\
	memcpy((JCTL)->jnl_fn + (NEW_PATH)->len + 1, CPTR_LAST, (CPTR - CPTR_LAST));	\
	(JCTL)->jnl_fn_len = (NEW_PATH)->len + (CPTR - CPTR_LAST) + 1;			\
} MBEND

#define OVERRIDE_JNL_PATH(JCTL, FILENAME, FILENAME_LEN, NEW_PATH)			\
MBSTART {										\
	int			filename_len;						\
	unsigned char		*last_slash;						\
											\
	for (last_slash = FILENAME + FILENAME_LEN - 1;					\
			last_slash >= FILENAME; --last_slash)				\
	{										\
		if ('/' == *last_slash)							\
			break;								\
	}										\
	assert(last_slash >= FILENAME);							\
	filename_len = FILENAME + FILENAME_LEN - last_slash;				\
	assert(0 < filename_len);							\
	memcpy((JCTL)->jnl_fn, (NEW_PATH)->buff, (NEW_PATH)->len);			\
	memcpy((JCTL)->jnl_fn + (NEW_PATH)->len, last_slash, filename_len);		\
	(JCTL)->jnl_fn_len = (NEW_PATH)->len + filename_len;				\
} MBEND

#define IGNORE_JNL_PATH(PTR, JFH, ADDR)							\
MBSTART {										\
	unsigned char		*ptr_fname2;						\
											\
	for (PTR = (JFH)->data_file_name + (JFH)->data_file_name_length - 1,		\
		ptr_fname2 = (ADDR)->fname + (ADDR)->fname_len - 1;			\
			PTR >= (JFH)->data_file_name; --PTR, --ptr_fname2)		\
	{										\
		if (*PTR != *ptr_fname2)						\
		{									\
			PTR = NULL;							\
			break;								\
		}									\
		if ('/' == *PTR)							\
			break;								\
	}										\
	assert(ptr_fname2 >= (ADDR)->fname);						\
} MBEND

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
