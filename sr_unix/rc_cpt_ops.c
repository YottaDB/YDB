/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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
#include "gtm_ipc.h"

#include <sys/sem.h>
#include <sys/shm.h>
#include <errno.h>

#include "iosp.h"
#include "rc_cpt.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "io.h"
#include "copy.h"

#include "rc.h"
#include "gtm_c_stack_trace.h"
#include "eintr_wrappers.h"
#include "eintr_wrapper_semop.h"
#include "rc_cpt_ops.h"
#include "do_shmat.h"
#include "trans_log_name.h"

GBLDEF	trans_num		rc_read_stamp;

GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	gd_region		*gv_cur_region;
GBLREF	bool			rc_locked;

error_def(ERR_DBFILERR);
error_def(ERR_TEXT);

static int		rc_shmid = INVALID_SHMID, rc_sem = INVALID_SEMID;
static rc_cp_table	*rc_cpt = 0;

static int rc_init_ipc(void);
static void rc_cpt_unlock(void);
static void rc_cpt_lock(void);

#define HIGH_WORD_SHIFT 0x00010000
#define HIGH_4BIT_SHIFT 0x10000000
#define LOW_WORD_MASK	0x0000FFFF
#define LOW_BYTE_MASK	0x000000FF
#define HIGH_BYTE_MASK  0x0000FF00

int rc_cpt_entry(int blk)
{
	key_t		rc_key;
	int4		entry;
	int		i;
	mstr		fpath1, fpath2;
	struct sembuf	sop[2];
	bool		found;

#	ifdef DEBUG_CPT
	FPRINTF(stderr,"\trc_cpt_entry(%d)",blk);
#	endif
	/* test any existing RC semaphore first */
	if (rc_sem)
	{
		errno = 0;
		i = semctl(rc_sem,0,GETVAL);
		if (errno)   /* invalid semaphore */
		{
			rc_sem = 0;
			/* detach shared memory segment as well */
			if (rc_cpt)
			{
				(void) shmdt((char *)rc_cpt);
				rc_cpt = NULL;
			}
		}
	}
	if (!rc_cpt)
	{
		if (i = rc_init_ipc())
			return i;	/* return error code (errno) */
	}
	/* cpvfy is the value of cpsync when the CPT table was last sent to a client.
	 * If this block has already been entered since then, do not reenter.
	 */
/*	entry = (((cs_data->dsid / 256) * 9) % LOW_BYTE_MASK) + (blk % LOW_WORD_MASK) + (cs_data->rc_node * HIGH_WORD_SHIFT); */
	entry = ((cs_data->dsid / 256 * 9 + blk) & LOW_BYTE_MASK) + (blk & HIGH_BYTE_MASK)  + (cs_data->rc_node * HIGH_WORD_SHIFT);
	found = FALSE;
	rc_cpt_lock();
	i = rc_cpt->index - (rc_cpt->cpsync - rc_cpt->cpvfy);
	while (i < 0)
		i += RC_CPT_TABSIZE;
	for (; i < RC_CPT_TABSIZE; i++)
	{
		if (i == rc_cpt->index)
			break;
		if (rc_cpt->ring_buff[i] == entry)
		{
			found = TRUE;
			break;
		}
	}
	if (i == RC_CPT_TABSIZE)
	{
		for (i = 0; i < rc_cpt->index; i++)
		{
			if (rc_cpt->ring_buff[i] == entry)
			{
				found = TRUE;
				break;
			}
		}
	}
#	ifdef DEBUG_CPT
	if (found)
	    FPRINTF(stderr," exists");
	else
	    FPRINTF(stderr," add");
#	endif
	if (!found)
	{
		rc_cpt->ring_buff[rc_cpt->index++] = entry;
		rc_cpt->cpsync++;
	}
	if (rc_cpt->index == RC_CPT_TABSIZE)
		rc_cpt->index = 0;
	rc_cpt_unlock();
#	ifdef DEBUG_CPT
	FPRINTF(stderr,"\n");
#	endif
	return 0;
}

/* set up shared memory and semaphore segments, clearing away any existing RC IPC */
static int rc_init_ipc(void)
{
	key_t		rc_key;
	mstr		fpath1, fpath2;
	int		old_errno;
	char		buff[1024];

	/* RC semaphores are automatically cleared by shmclean, but it
	 * is necessary to clear any previously existing shared memory.
	 */
	if (rc_cpt)
	{
		(void) shmdt((char *)rc_cpt);
		rc_cpt = NULL;
	}
	fpath1.addr = RC_CPT_PATH;
	fpath1.len = SIZEOF(RC_CPT_PATH);
	if (SS_NORMAL != TRANS_LOG_NAME(&fpath1, &fpath2, buff, SIZEOF(buff), do_sendmsg_on_log2long))
	{
		PERROR("Error translating rc path");
		return errno;
	}
	if ((rc_key = FTOK(fpath2.addr,GTM_ID)) == -1)
	{
		PERROR("Error with rc ftok");
		return errno;
	}
	if ((rc_shmid = shmget(rc_key, SIZEOF(rc_cp_table) , RWDALL)) == -1)
	{
		rc_shmid = INVALID_SHMID;
		PERROR("Error with rc shmget");
		return errno;
	}
	if ((INTPTR_T)(rc_cpt = (rc_cp_table*)do_shmat(rc_shmid, 0, 0)) == -1L)
	{
		PERROR("Error with rc shmat");
		return errno;
	}
	if ((rc_sem = semget(rc_key, 4, RWDALL)) == -1)
	{
		old_errno = errno;
		PERROR("Error with rc semget");
		(void) shmdt((char *)rc_cpt);
		rc_cpt = 0;
		rc_sem = INVALID_SEMID;
		return old_errno;
	}
	return 0;
}

static void rc_cpt_unlock(void)
{	struct sembuf	sop[2];
	int		rv;
	int		semop_rv;

	sop[0].sem_num = 0;
	sop[0].sem_op = -1;
	sop[0].sem_flg = SEM_UNDO;
	rc_locked = FALSE;
	SEMOP(rc_sem, sop, 1, semop_rv, NO_WAIT);
	if (-1 == semop_rv)
	{
		if (errno == EINVAL)
		{	/* try reinitializing semaphore... */
			if (!(rv=rc_init_ipc()))
			{
				SEMOP(rc_sem, sop, 1, semop_rv, NO_WAIT);
				if (-1 != semop_rv)
				{
					rc_locked = FALSE;
					return;
				}
			}
		}
		if (gv_cur_region)
			rts_error(VARLSTCNT(9) ERR_DBFILERR,2, DB_LEN_STR(gv_cur_region),
				  ERR_TEXT, 2, LEN_AND_LIT("Error with rc semaphore unlock"), errno);
		else
			rts_error(VARLSTCNT(5) ERR_TEXT, 2, LEN_AND_LIT("Error with rc semaphore unlock"), errno);
	}
	return;
}

static void rc_cpt_lock(void)
{
	struct sembuf	sop[2];
	int		rv;
	int		semop_rv;

/* WARNING:  To prevent deadlocks, never attempt to acquire a database critical section while holding this semaphore */

	sop[0].sem_num = sop[1].sem_num = 0;
	sop[0].sem_op = 0;
	sop[1].sem_op = 1;
	sop[0].sem_flg = sop[1].sem_flg = SEM_UNDO;
	SEMOP(rc_sem, sop, 2, semop_rv, FORCED_WAIT)
	if (-1 == semop_rv)
	{
		if (errno == EINVAL)
		{	/* try reinitializing semaphore... */
			if (!(rv=rc_init_ipc()))
			{
				SEMOP(rc_sem, sop, 2, semop_rv, FORCED_WAIT);
				if (-1 != semop_rv)
				{
					rc_locked = TRUE;
					return;
				}
			}
		}
		if (gv_cur_region)
			rts_error(VARLSTCNT(9) ERR_DBFILERR,2, DB_LEN_STR(gv_cur_region),
				  ERR_TEXT, 2, LEN_AND_LIT("Error with rc semaphore lock"), errno);
		else
			rts_error(VARLSTCNT(5) ERR_TEXT, 2, LEN_AND_LIT("Error with rc semaphore lock"), errno);
	}
	rc_locked = TRUE;
	return;
}

int rc_cpt_inval(void)
{
	key_t		rc_key;
	mstr		fpath1, fpath2;
	int4		entry;
	struct sembuf	sop[2];
	int		i, old_errno;
	char		buff[1024];
	bool		found;

#	ifdef DEBUG_CPT
	FPRINTF(stderr,"\trc_cpt_inval()\n");
#	endif
	if (!rc_cpt)
	{	fpath1.addr = RC_CPT_PATH;
		fpath1.len = SIZEOF(RC_CPT_PATH);
		if (SS_NORMAL != TRANS_LOG_NAME(&fpath1, &fpath2, buff, SIZEOF(buff), do_sendmsg_on_log2long))
		{
			PERROR("Error translating rc path");
			return errno;
		}
		if ((rc_key = FTOK(fpath2.addr,GTM_ID)) == -1)
		{
			PERROR("Error with rc ftok");
			return errno;
		}
		if ((rc_shmid = shmget(rc_key, SIZEOF(rc_cp_table) , RWDALL)) == -1)
		{
			rc_shmid = INVALID_SHMID;
			PERROR("Error with rc shmget");
			return errno;
		}
		if ((INTPTR_T)(rc_cpt = (rc_cp_table*)do_shmat(rc_shmid, 0, 0)) == -1L)
		{
			PERROR("Error with rc shmat");
			return errno;
		}
		if ((rc_sem = semget(rc_key, 4, RWDALL)) == -1)
		{
			old_errno = errno;
			PERROR("Error with rc semget");
			(void) shmdt((char *)rc_cpt);
			rc_cpt = 0;
			rc_sem = INVALID_SEMID;
			return old_errno;
		}
	}
	rc_cpt_lock();
	entry = cs_data->dsid + (cs_data->rc_node * HIGH_WORD_SHIFT) + (RC_CPT_INVAL * HIGH_4BIT_SHIFT);
	rc_cpt->ring_buff[rc_cpt->index++] = entry;
	rc_cpt->cpsync++;
	if (rc_cpt->index == RC_CPT_TABSIZE)
		rc_cpt->index = 0;
	rc_cpt_unlock();
	return 0;
}

void rc_close_section(void)
{
#	ifndef GTCM_RC
	if (rc_cpt)
	{	(void) shmdt((char *)rc_cpt);
		rc_cpt = 0;
	}
#	endif
	return;
}

/* mupip_rundown_cpt()
 *
 * Called from mupip rundown to force the deletion of the RC cpt, if
 * present
 *
 *	returns:
 *		zero		GT.CM installed and CPT deleted or not present or
 *				GT.CM not installed
 *
 *		non-zero	CPT present, but could not be deleted.
 *
 * The CPT is not deleted if there are other processes attached to it.
 */
int mupip_rundown_cpt()
{
	char		buff[1024];
	key_t		rc_key;
	mstr		fpath1, fpath2;
	struct shmid_ds	shm_buf;

	/* detach from CPT if we happen to be connected */
	if (rc_cpt)
		(void) shmdt((char *)rc_cpt);
	fpath1.addr = RC_CPT_PATH;
	fpath1.len = SIZEOF(RC_CPT_PATH);
	if (SS_NORMAL != TRANS_LOG_NAME(&fpath1, &fpath2, buff, SIZEOF(buff), do_sendmsg_on_log2long))
	{	/* invalid environment variable setup....error */
		return -1;
	}
	if ((rc_key = FTOK(fpath2.addr,GTM_ID)) == -1)
	{	/* no GT.CM server installed on system - okay to reset RC values */
		return 0;
	}
	if ((rc_shmid = shmget(rc_key, SIZEOF(rc_cp_table), RWDALL)) == -1)
	{	/* no RC CPT - okay to reset RC values */
		rc_shmid = INVALID_SHMID;
		return 0;
	}
	if (shmctl(rc_shmid, IPC_STAT, &shm_buf) == -1)
	{
		PERROR("Warning- can't access RC CPT");
		return -1;
	}
	if (shm_buf.shm_nattch == 0) /* no GT.CM servers out there */
	{	/* delete CPT shared memory */
		if (shmctl(rc_shmid, IPC_RMID, 0) == -1)
		{
			PERROR("Warning- can't delete RC CPT");
			return -1;
		}
		/* attach to and delete CPT semaphore */
		if ((rc_sem = semget(rc_key, 4, RWDALL)) == -1)
		{
			PERROR("Warning- can't access RC CPT semaphore");
			rc_cpt = 0;
			rc_sem = INVALID_SEMID;
			return 0;
		}
		if (semctl(rc_sem, 0, IPC_RMID, 0) == -1)
		{
			PERROR("Error cleaning up RC CPT semaphore");
		}
		return 0;  /* we successfully deleted CPT shared memory */
	}
	return -1;
}

void rc_delete_cpt(void)
{
	struct sembuf	sop[2];
	struct shmid_ds	shm_buf;
	int		semop_rv;

	if (!rc_cpt)
		return;
	sop[0].sem_num = sop[1].sem_num = 0;
	sop[0].sem_op = 0;
	sop[1].sem_op = 1;
	sop[0].sem_flg = sop[1].sem_flg = SEM_UNDO;
	SEMOP(rc_sem, sop, 2, semop_rv, FORCED_WAIT);
	if (-1 == semop_rv)
	{
		PERROR("Error with RC semaphore lock");
		(void) shmdt((char *)rc_cpt);
		rc_cpt = 0;
		rc_sem = 0;
		return;
	}
	if (rc_cpt->server_count == 1)	/* only daemon, delete it */
	{
		if (shmctl(rc_shmid,IPC_RMID,&shm_buf) == -1)
		{
			PERROR("Error cleaning up rc shared memory segment");
		}
		if (semctl(rc_sem, 0, IPC_RMID, 0) == -1)
		{
			PERROR("Error cleaning up rc semaphores");
		}
	} else
	{
		rc_cpt->server_count--;
		sop[0].sem_num = 0;
		sop[0].sem_op = -1;
		sop[0].sem_flg = SEM_UNDO;
		SEMOP(rc_sem, sop, 1, semop_rv, NO_WAIT);
		if (-1 == semop_rv)
		{
			PERROR("Error with RC semaphore unlock");
			(void) shmdt((char *)rc_cpt);
			rc_cpt = 0;
			rc_sem = 0;
			if (semctl(rc_sem, 0, IPC_RMID, 0) == -1)
			{
				PERROR("Error cleaning up rc semaphores");
			}
		}
	}
	return;
}

/****************************** Routines used only by the server **********************************/

int rc_create_cpt(void)
{
	char		buff[1024];
	int		old_errno;
	int		semop_rv;
	key_t		rc_key;
	mstr		fpath1, fpath2;
	struct sembuf	sop[2];
	struct shmid_ds	shm_buf;

	if (rc_cpt)
		return 0;
	fpath1.addr = RC_CPT_PATH;
	fpath1.len = SIZEOF(RC_CPT_PATH);
	if (SS_NORMAL != TRANS_LOG_NAME(&fpath1, &fpath2, buff, SIZEOF(buff), do_sendmsg_on_log2long))
	{
		PERROR("Error translating rc path");
		return errno;
	}
	if ((rc_key = FTOK(fpath2.addr,GTM_ID)) == -1)
	{
		PERROR("Error with rc ftok");
		return errno;
	}
	if ((rc_shmid = shmget(rc_key, SIZEOF(rc_cp_table) ,IPC_CREAT |  RWDALL)) == -1)
	{
		rc_shmid = INVALID_SHMID;
		PERROR("Error with rc shmget");
		return errno;
	}
	if ((INTPTR_T)(rc_cpt = (rc_cp_table*)do_shmat(rc_shmid, 0, 0)) == -1L)
	{
		PERROR("Error with rc shmat");
		return errno;
	}
	if ((rc_sem = semget(rc_key, 4, IPC_CREAT | RWDALL)) == -1)
	{
		old_errno = errno;
		PERROR("Error with rc semget");
		(void) shmdt((char *)rc_cpt);
		rc_cpt = 0;
		rc_sem = INVALID_SEMID;
		return old_errno;
	}
	sop[0].sem_num = sop[1].sem_num = 0;
	sop[0].sem_op = 0;
	sop[1].sem_op = 1;
	sop[0].sem_flg = sop[1].sem_flg = SEM_UNDO;
	SEMOP(rc_sem, sop, 2, semop_rv, FORCED_WAIT);
	if (-1 == semop_rv)
	{
		old_errno = errno;
		PERROR("Error with RC semaphore lock");
		(void) shmdt((char *)rc_cpt);
		rc_cpt = 0;
		rc_sem = 0;
		return old_errno;
	}
	rc_cpt->server_count++;
	sop[0].sem_num = 0;
	sop[0].sem_op = -1;
	sop[0].sem_flg = SEM_UNDO;
	SEMOP(rc_sem, sop, 1, semop_rv, NO_WAIT);
	if (-1 == semop_rv)
	{
		old_errno = errno;
		PERROR("Error with RC semaphore unlock");
		rc_cpt->server_count++;
		(void) shmdt((char *)rc_cpt);
		rc_cpt = 0;
		rc_sem = 0;
		if (semctl(rc_sem, 0, IPC_RMID, 0) == -1)
		{
			PERROR("Error cleaning up rc semaphores");
		}
		return old_errno;
	}
	return 0;
}

short rc_get_cpsync(void)
{
	return rc_cpt->cpsync;
}

/* Copy CPT into XBLK, check for overflow, verify that if there is a getrecord operation, the page is still valid */
void rc_send_cpt(rc_xblk_hdr *head, rc_rsp_page *last_aq)	/* Zero if no read op in this XBLK */
{	char		*ptr;
	int		cpt_size, copy_size;
	bt_rec_ptr_t	b;

	if (!rc_cpt)
	{
		head->free.value = head->cpt_tab.value;
		head->cpt_siz.value = 0;
		return;
	}
	if (last_aq && (last_aq->hdr.r.typ.value == RC_GET_PAGE || last_aq->hdr.r.typ.value == RC_GET_RECORD))
	{
		int4 blknum;

		assert(!cs_addrs->hold_onto_crit);	/* this ensures we can safely do unconditional grab_crit and rel_crit */
		grab_crit(gv_cur_region); /* DBP Need to verify that block hasn't changed since copied.  look in BT? */
		rc_cpt_lock();
		GET_LONG(blknum, last_aq->pageaddr);
		if ((b = bt_get(blknum)) ? rc_read_stamp <= b->tn :
			(rc_read_stamp <= OLDEST_HIST_TN(cs_addrs)))
		{
			last_aq->hdr.a.erc.value = RC_NETERRRETRY;
		}
		rel_crit(gv_cur_region);
	} else
		rc_cpt_lock();
	cpt_size = rc_cpt->cpsync - head->sync.value;
	if (cpt_size < 0)			/* handle wraps */
		cpt_size += RC_MAX_CPT_SYNC;
	cpt_size *= (int)(RC_CPT_ENTRY_SIZE);
	if ((cpt_size > (int)head->cpt_siz.value) || (cpt_size > (RC_CPT_ENTRY_SIZE * RC_CPT_TABSIZE)))
	{
		head->cpt_siz.value = RC_CPT_OVERFLOW;
		rc_cpt->cpvfy = rc_cpt->cpsync;
		head->sync.value = rc_cpt->cpsync;
		head->free.value = head->cpt_tab.value;
		rc_cpt_unlock();
		return;
	} else
	{
		head->cpt_siz.value = cpt_size;
		ptr = (char *)((char *)&rc_cpt->ring_buff[rc_cpt->index] - cpt_size);
		if (ptr < (char *)rc_cpt->ring_buff)
		{
			ptr = (char *)rc_cpt->ring_buff;
			copy_size = (int4)((char *)&rc_cpt->ring_buff[rc_cpt->index] - (char *)rc_cpt->ring_buff);
		} else
			copy_size = cpt_size;
		memcpy((char *)head + head->cpt_tab.value, ptr, copy_size);
		if (copy_size != cpt_size)
		{
			memcpy((char *)head + head->cpt_tab.value + copy_size,
				(char *)ARRAYTOP(rc_cpt->ring_buff) - (cpt_size - copy_size), cpt_size - copy_size);
		}
	}
	rc_cpt->cpvfy = rc_cpt->cpsync;
	head->sync.value = rc_cpt->cpsync;
	head->free.value = head->cpt_tab.value + head->cpt_siz.value;
	rc_cpt_unlock();
	return;
}
