/****************************************************************
 *								*
 * Copyright (c) 2014-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <sys/shm.h>
#include <sys/mman.h>

#include "gtm_ipc.h"
#include "gtm_string.h"
#include "gtm_stdio.h"

#include "rtnobj.h"
#include "gtmio.h"
#include <rtnhdr.h>
#include "iosp.h"
#include "do_shmat.h"
#include "min_max.h"
#include "relqop.h"
#include "ipcrmid.h"
#include "interlock.h"
#include "gdsroot.h"	/* for CDB_STAGNATE */
#include "incr_link.h"	/* for NATIVE_HDR_LEN */
#include "hugetlbfs_overrides.h"	/* for the ADJUST_SHM_SIZE_FOR_HUGEPAGES macro */
#include "gtm_permissions.h"
#include "cachectl.h"
#include "cacheflush.h"

/* Uncomment below line if you want RTNOBJ_DBG lines to be enabled */
/* #define RTNOBJ_DEBUG */

#ifdef RTNOBJ_DEBUG
#	define RTNOBJ_DBG(X)	X
#else
#	define RTNOBJ_DBG(X)
#endif

#define	DBG_ASSERT_INDEX_VALID(MIN_INDEX, MAX_INDEX)		\
{								\
	assert(0 <= MIN_INDEX);					\
	assert(NUM_RTNOBJ_SHM_INDEX >= MIN_INDEX);		\
	assert(0 <= MAX_INDEX);					\
	assert(NUM_RTNOBJ_SHM_INDEX >= MAX_INDEX);		\
}

/* Macro to make sure this process has attached to ALL rtnobj shared memory segments that currently exist for the
 * relinkctl shared memory corresponding to LINKCTL. Need to execute this macro before checking if a given routine object
 * does exist in one of the rtnobj shared memory segments.
 */
#define	SYNC_RTNOBJ_SHMID_CYCLE_IF_NEEDED(LINKCTL, MIN_INDEX, MAX_INDEX, SHM_HDR, HAS_RELINKCTL_LOCK, RELINKREC)		\
{																\
	int		pvt_max_index, i, pvt_shmid, shr_shmid;									\
	int		save_errno;												\
	size_t		shm_size;												\
	sm_uc_ptr_t	shm_base;												\
	char		errstr[256];												\
																\
	MAX_INDEX = SHM_HDR->rtnobj_max_shm_index;										\
	SHM_READ_MEMORY_BARRIER;	/* Read "rtnobj_max_shm_index" BEFORE read memory barrier and everything else		\
					 * else associated with this index AFTER the memory barrier.				\
					 */											\
	pvt_max_index = LINKCTL->rtnobj_max_shm_index;										\
	DEBUG_ONLY(														\
		for (i = LINKCTL->rtnobj_min_shm_index; i < pvt_max_index; i++)							\
		{														\
			pvt_shmid = LINKCTL->rtnobj_shmid[i];									\
			shr_shmid = SHM_HDR->rtnobj_shmhdr[i].rtnobj_shmid;							\
			assert(pvt_shmid == shr_shmid);										\
		}														\
	)															\
	if (pvt_max_index != MAX_INDEX)												\
	{															\
		MIN_INDEX = SHM_HDR->rtnobj_min_shm_index;									\
		DBG_ASSERT_INDEX_VALID(MIN_INDEX, MAX_INDEX); /* assert validity of min_index/max_index */			\
		for (i = pvt_max_index; i < MAX_INDEX; i++)									\
		{														\
			pvt_shmid = LINKCTL->rtnobj_shmid[i];									\
			shr_shmid = SHM_HDR->rtnobj_shmhdr[i].rtnobj_shmid;							\
			if (pvt_shmid != shr_shmid)										\
			{													\
				assert(INVALID_SHMID == pvt_shmid); /* currently rtnobj shmids are never deleted */		\
				shm_size = ((size_t)1 << (i + MIN_RTNOBJ_SHM_INDEX));						\
				shm_base = (sm_uc_ptr_t)do_shmat_exec_perm(shr_shmid, shm_size, &save_errno);			\
				if (-1 == (sm_long_t)shm_base)									\
				{												\
					if (HAS_RELINKCTL_LOCK)									\
						rel_latch(&SHM_HDR->relinkctl_latch);						\
					rel_latch(&RELINKREC->rtnobj_latch);							\
					SNPRINTF(errstr, SIZEOF(errstr), "rtnobj2 shmat() failed for shmid=%d shmsize=0x%llx",	\
						shr_shmid, shm_size);								\
					if (!SHM_REMOVED(save_errno))								\
						ISSUE_RELINKCTLERR_SYSCALL(&LINKCTL->zro_entry_name, errstr, save_errno);	\
					else											\
						ISSUE_REQRLNKCTLRNDWN_SYSCALL(LINKCTL, errstr, save_errno);			\
				}												\
				assert(0 == ((UINTPTR_T)shm_base % 8));								\
				LINKCTL->rtnobj_shmid[i] = shr_shmid;								\
				LINKCTL->rtnobj_shm_base[i] = shm_base;								\
			}													\
		}														\
		LINKCTL->rtnobj_min_shm_index = MIN_INDEX;									\
		LINKCTL->rtnobj_max_shm_index = MAX_INDEX;									\
	} else															\
	{															\
		MIN_INDEX = LINKCTL->rtnobj_min_shm_index;									\
		MAX_INDEX = pvt_max_index;											\
		DBG_ASSERT_INDEX_VALID(MIN_INDEX, MAX_INDEX);									\
	}															\
}

#ifdef AUTORELINK_SUPPORTED

error_def(ERR_RELINKCTLERR);
error_def(ERR_RLNKRECLATCH);
error_def(ERR_RLNKSHMLATCH);
error_def(ERR_SYSCALL);

DEBUG_ONLY(GBLREF int	saved_errno;)
DEBUG_ONLY(GBLREF int	process_exiting;)

#ifdef DEBUG
/* Routine to verify the doubly-linked (fl/bl) freelists for each rtnobj size (various 2-powers) are in good shape */
void	rtnobj_verify_freelist_fl_bl(rtnobjshm_hdr_t *rtnobj_shm_hdr, sm_uc_ptr_t shm_base)
{
	int		min_index, max_index, i;
	que_ent_ptr_t	freeList, elem, prev_elem;
	rtnobj_hdr_t	*rtnobj, *rtnobj2;
	sm_off_t	rtnobj_off;
	gtm_uint64_t	elemSize;
	sm_uc_ptr_t	shm_top;

	freeList = &rtnobj_shm_hdr->freeList[0];
	shm_top = shm_base + rtnobj_shm_hdr->shm_len;
	for (i = 0; i < NUM_RTNOBJ_SIZE_BITS; i++, freeList++)
	{
		if (NULL_RTNOBJ_SM_OFF_T != freeList->fl)
		{
			assert(NULL_RTNOBJ_SM_OFF_T != freeList->bl);
			assert(0 == freeList->fl % 8);
			prev_elem = NULL;
			elem = (que_ent_ptr_t)(shm_base + freeList->fl);
			do
			{
				rtnobj = (rtnobj_hdr_t *)((sm_uc_ptr_t)elem - OFFSETOF(rtnobj_hdr_t, userStorage));
				assert(STATE_FREE == rtnobj->state);
				assert(i == rtnobj->queueIndex);
				rtnobj_off = (sm_uc_ptr_t)rtnobj - shm_base;
				elemSize = ((gtm_uint64_t)1 << (rtnobj->queueIndex + MIN_RTNOBJ_SIZE_BITS));
				rtnobj2 = (rtnobj_hdr_t *)(shm_base + (rtnobj_off ^ elemSize));	/* address of buddy */
				assert(((sm_uc_ptr_t)rtnobj2 == shm_top) || (rtnobj2->queueIndex <= rtnobj->queueIndex));
				if (NULL_RTNOBJ_SM_OFF_T == elem->bl)
					assert(NULL == prev_elem);
				else
					assert(((NULL == prev_elem) && (NULL_RTNOBJ_SM_OFF_T == elem->bl))
						|| (prev_elem->fl == -elem->bl));
				if (NULL_RTNOBJ_SM_OFF_T == elem->fl)
				{
					assert(elem == (que_ent_ptr_t)(shm_base + freeList->bl));
					break;
				}
				assert(0 == elem->fl % 8);
				prev_elem = elem;
				elem = (que_ent_ptr_t)((sm_uc_ptr_t)elem + elem->fl);
			}
			while (TRUE);
		} else
			assert(NULL_RTNOBJ_SM_OFF_T == freeList->bl);
	}
}

/* Routine to verify the correctness of derived fields, rtnobj_shm_hdr->rtnobj_min_free_index & rtnobj->rtnobj_max_free_index */
void	rtnobj_verify_min_max_free_index(rtnobjshm_hdr_t *rtnobj_shm_hdr)
{
	int		min_index, max_index, i;
	que_ent_ptr_t	freeList;

	min_index = NUM_RTNOBJ_SIZE_BITS;
	max_index = 0;
	freeList = &rtnobj_shm_hdr->freeList[0];
	for (i = 0; i < NUM_RTNOBJ_SIZE_BITS; i++, freeList++)
	{
		if (NULL_RTNOBJ_SM_OFF_T != freeList->fl)
		{
			if (min_index > i)
				min_index = i;
			if (max_index < (i + 1))
				max_index = i + 1;
		}
	}
	assert(min_index == rtnobj_shm_hdr->rtnobj_min_free_index);
	assert(max_index == rtnobj_shm_hdr->rtnobj_max_free_index);
}
#endif

/* Insert "new_tail" at tail of the doubly-linked relative-queue starting from "que_base".
 * Note "que_base" is a pointer to relinkctl shared memory whereas "new_tail" is a pointer to rtnobj shared memory.
 * Hence the need for NULL_RTNOBJ_SM_OFF_T to indicate a link from rtnobj shm to relinkctl shm.
 * "shm_base" is pointer to the start of rtnobj shared memory. Used to calculate relative offsets.
 */
void	insqt_rtnobj(que_ent_ptr_t new_tail, que_ent_ptr_t que_base, sm_uc_ptr_t shm_base)
{
	que_ent_ptr_t old_tail;

	RTNOBJ_DBG(fprintf(stderr, "insqt_rtnobj : que_base = 0x%llx : shm_base = 0x%llx : elem = 0x%llx\n",		\
		(UINTPTR_T)que_base, (UINTPTR_T)shm_base, (UINTPTR_T)new_tail);)
	if (NULL_RTNOBJ_SM_OFF_T == que_base->bl)
	{	/* Queue has nothing */
		assert(NULL_RTNOBJ_SM_OFF_T == que_base->fl);
		que_base->fl = (uchar_ptr_t)new_tail - (uchar_ptr_t)shm_base;
		assert(0 == que_base->fl % 8);
		que_base->bl = que_base->fl;
		new_tail->fl = NULL_RTNOBJ_SM_OFF_T;
		new_tail->bl = NULL_RTNOBJ_SM_OFF_T;
	} else
	{
		old_tail = (que_ent_ptr_t)(shm_base + que_base->bl);
		assert(NULL_RTNOBJ_SM_OFF_T == old_tail->fl);
		new_tail->fl = NULL_RTNOBJ_SM_OFF_T;
		new_tail->bl = (uchar_ptr_t)old_tail - (uchar_ptr_t)new_tail;
		assert(0 == new_tail->bl % 8);
		old_tail->fl = -new_tail->bl;
		que_base->bl += old_tail->fl;
	}
	return;
}

/* Remove an element from the head of the doubly-linked relative-queue starting from "que_base" and return a pointer to it.
 * Note "que_base" is a pointer to relinkctl shared memory whereas the returned value is a pointer to rtnobj shared memory.
 * Hence the need for NULL_RTNOBJ_SM_OFF_T to indicate a link from rtnobj shm to relinkctl shm.
 * "shm_base" is pointer to the start of rtnobj shared memory. Used to calculate relative offsets.
 */
rtnobj_hdr_t *remqh_rtnobj(que_ent_ptr_t que_base, sm_uc_ptr_t shm_base)
{
	que_ent_ptr_t	ret, new_head;
	rtnobj_hdr_t	*rtnobj;

	assert(NULL_RTNOBJ_SM_OFF_T != que_base->fl);
	ret = (que_ent_ptr_t)(shm_base + que_base->fl);
	RTNOBJ_DBG(fprintf(stderr, "remqh_rtnobj : que_base = 0x%llx : shm_base = 0x%llx : elem = 0x%llx\n",		\
		(UINTPTR_T)que_base, (UINTPTR_T)shm_base, (UINTPTR_T)ret);)
	assert(0 == (sm_off_t)ret % 8);
	assert(NULL_RTNOBJ_SM_OFF_T == ret->bl);
	if (NULL_RTNOBJ_SM_OFF_T == ret->fl)
	{	/* Empty queue after this removal */
		que_base->fl = NULL_RTNOBJ_SM_OFF_T;
		que_base->bl = NULL_RTNOBJ_SM_OFF_T;
	} else
	{
		new_head = (que_ent_ptr_t)((uchar_ptr_t)ret + ret->fl);
		assert(0 == (sm_off_t)new_head % 8);
		new_head->bl = NULL_RTNOBJ_SM_OFF_T;
		assert(0 == que_base->fl % 8);
		que_base->fl += ret->fl;
	}
	rtnobj = (rtnobj_hdr_t *)((sm_uc_ptr_t)ret - OFFSETOF(rtnobj_hdr_t, userStorage));
	return rtnobj;
}

/* Remove a specific element "rtnobj" from wherever it is in the doubly-linked relative-queue starting from "que_base".
 * Note "que_base" is a pointer to relinkctl shared memory whereas "rtnobj" is a pointer to rtnobj shared memory.
 * Hence the need for NULL_RTNOBJ_SM_OFF_T to indicate a link from rtnobj shm to relinkctl shm.
 * "shm_base" is pointer to the start of rtnobj shared memory. Used to calculate relative offsets.
 */
void	remq_rtnobj_specific(que_ent_ptr_t que_base, sm_uc_ptr_t shm_base, rtnobj_hdr_t *rtnobj)
{
	que_ent_ptr_t	elem, new_head, rtnque;

	assert(NULL_RTNOBJ_SM_OFF_T != que_base->fl);
	rtnque = (que_ent_ptr_t)((sm_uc_ptr_t)rtnobj + OFFSETOF(rtnobj_hdr_t, userStorage));
	RTNOBJ_DBG(fprintf(stderr, "remqh_rtnobj_specific : que_base = 0x%llx : shm_base = 0x%llx : elem = 0x%llx\n",	\
		(UINTPTR_T)que_base, (UINTPTR_T)shm_base, (UINTPTR_T)rtnque);)
#	ifdef DEBUG
	/* Verify that "rtnobj" is actually in the queue first */
	elem = (que_ent_ptr_t)(shm_base + que_base->fl);
	assert(NULL_RTNOBJ_SM_OFF_T == elem->bl);
	while (elem != rtnque)
	{
		assert(NULL_RTNOBJ_SM_OFF_T != elem->fl);
		elem = (que_ent_ptr_t)((sm_uc_ptr_t)elem + elem->fl);
	}
#	endif
	if (NULL_RTNOBJ_SM_OFF_T == rtnque->bl)
	{	/* rtnobj is at head of queue */
		if (NULL_RTNOBJ_SM_OFF_T == rtnque->fl)
		{	/* rtnobj is only element in queue */
			que_base->fl = NULL_RTNOBJ_SM_OFF_T;
			que_base->bl = NULL_RTNOBJ_SM_OFF_T;
		} else
		{
			elem = (que_ent_ptr_t)((uchar_ptr_t)rtnque + rtnque->fl);
			assert(0 == (sm_off_t)elem % 8);
			elem->bl = NULL_RTNOBJ_SM_OFF_T;
			assert(0 == que_base->fl % 8);
			assert(0 == rtnque->fl % 8);
			que_base->fl += rtnque->fl;
		}
	} else if (NULL_RTNOBJ_SM_OFF_T == rtnque->fl)
	{	/* rtnobj is at tail of queue with at least one more element in queue */
		elem = (que_ent_ptr_t)((uchar_ptr_t)rtnque + rtnque->bl);
		elem->fl = NULL_RTNOBJ_SM_OFF_T;
		assert(0 == que_base->bl % 8);
		assert(0 == rtnque->bl % 8);
		que_base->bl += rtnque->bl;
	} else
	{	/* rtnobj is in middle of queue with at least one element in queue on either side */
		elem = (que_ent_ptr_t)((uchar_ptr_t)rtnque + rtnque->bl);
		assert(0 == (sm_off_t)elem % 8);
		assert(0 == elem->fl % 8);
		assert(0 == rtnque->fl % 8);
		elem->fl += rtnque->fl;
		assert(0 == (sm_off_t)rtnque % 8);
		assert(0 == rtnque->fl % 8);
		elem = (que_ent_ptr_t)((uchar_ptr_t)rtnque + rtnque->fl);
		assert(0 == elem->bl % 8);
		assert(0 == rtnque->bl % 8);
		elem->bl += rtnque->bl;
	}
}

/* Function to allocate "objSize" bytes of space, for the object file that "fd" points to and has a hash value "objhash",
 * in rtnobj shared memory for the relinkctl file/shared-memory derived from "zhist". If such an object already exists, return
 * a pointer to that location in rtnobj shared memory after incrementing reference-counts (to indicate how many processes
 * have linked in to this shared object). In this fast-path case, we hold a lock only on the relink record corresponding
 * to this object's routine-name. In the other case (where space has to be allocated), we also hold a lock on the entire
 * relinkctl shared memory (and effectively all rtnobj shared memory segments).
 */
sm_uc_ptr_t rtnobj_shm_malloc(zro_hist *zhist, int fd, off_t objSize, gtm_uint64_t objhash)
{
	boolean_t		has_relinkctl_lock, has_rtnobj_lock;
	boolean_t		initialized, return_NULL, need_shmctl;
	char			errstr[256];
	gtm_uint64_t		src_cksum_8byte;
	int			min_index, max_index, min_free_index, max_free_index, shm_index;
	int			objIndex, tSize, sizeIndex, shmSizeIndex, shmid, actualSize;
	int			codelen, loopcnt, save_errno;
	open_relinkctl_sgm	*linkctl;
	que_ent_ptr_t		freeList;
	relinkrec_t		*relinkrec;
	relinkshm_hdr_t		*shm_hdr;
	sm_uc_ptr_t		shm_base;
	rtnobj_hdr_t		*rtnobj, *prev_rtnobj, *rtnobj2;
	rtnobj_sm_off_t		shm_index_off;
	rtnobjshm_hdr_t		*rtnobj_shm_hdr;
	size_t			shm_size;
	off_t			rtnobjSize;
	sm_off_t		shm_off;
	sm_uc_ptr_t		objBuff, codeadr;
	rhdtyp			tmprhd, *rhdr;
	gtm_uint64_t		elemSize;
	zro_validation_entry	*zhent;
	struct stat		dir_stat_buf;
	int			stat_res;
	int			user_id;
	int			group_id;
	int			perm;
	int			maxvers, curvers;
	struct shmid_ds		shmstat;
	struct perm_diag_data	pdd;
#	ifdef DEBUG
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	endif
	zhent = zhist->end - 1;
	relinkrec = zhent->relinkrec;
	linkctl = zhent->relinkctl_bkptr;
	/* Assert certain design assumptions */
	assert((MAX_RTNOBJ_SHM_INDEX + RTNOBJ_SHMID_INDEX_MAXBITS) <= (8 * SIZEOF(rtnobj_sm_off_t)));
	assert(((size_t)1 << RTNOBJ_SHMID_INDEX_MAXBITS) >= MAX_RTNOBJ_SHM_INDEX);
	shm_hdr = GET_RELINK_SHM_HDR(linkctl);
	loopcnt = 0;
	has_relinkctl_lock = FALSE;
	assert(!linkctl->locked);
	assert(!linkctl->hdr->file_deleted);
	if (!grab_latch(&relinkrec->rtnobj_latch, RLNKREC_LATCH_TIMEOUT_SEC))
	{
		assert(FALSE);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5)
				ERR_RLNKRECLATCH, 3, relinkrec->rtnname_fixed.c, RTS_ERROR_MSTR(&linkctl->zro_entry_name));
	}
	do
	{
		if (CDB_STAGNATE <= loopcnt++)
		{	/* We have tried THRICE to search without a lock but found something inconsistent (due to concurrent
			 * changes to shared memory structures). Get a lock on the entire relinkctl file before searching
			 * for one last time.
			 */
			assert(!has_relinkctl_lock);
			if (!grab_latch(&shm_hdr->relinkctl_latch, RLNKSHM_LATCH_TIMEOUT_SEC))
			{
				assert(FALSE);
				rel_latch(&relinkrec->rtnobj_latch);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4)
						ERR_RLNKSHMLATCH, 2, RTS_ERROR_MSTR(&linkctl->zro_entry_name));
			}
			has_relinkctl_lock = TRUE;
		}
		/* Check if we are in sync with the rtnobj shared memory segments corresponding to this relinkctl shared memory.
		 * If not attach to those rtnobj shmids first.
		 */
		SYNC_RTNOBJ_SHMID_CYCLE_IF_NEEDED(linkctl, min_index, max_index, shm_hdr, has_relinkctl_lock, relinkrec);
		/* Now that we have attached to the necessary routine buffer shmids, search within shared memory for the
		 * routine buffer whose <objhash,objLen> matches ours.
		 */
		assert(MAXUINT4 >= objSize);
		shm_index_off = relinkrec->rtnobj_shm_offset;
		prev_rtnobj = NULL;
		maxvers = relinkrec->numvers;
		curvers = 0;
		while ((rtnobj_sm_off_t)NULL_RTNOBJ_SM_OFF_T != shm_index_off)
		{
			assertpro(maxvers >= curvers++);	/* assertpro to avoid infinite loops in PRO (just in case) */
			shm_index = RTNOBJ_GET_SHM_INDEX(shm_index_off);
			shm_off = RTNOBJ_GET_SHM_OFFSET(shm_index_off);
			assert(0 == (shm_off % 8));
			/* Since we hold the rtnobj_latch we should see valid values of shm_index/shm_off */
			assert(min_index <= shm_index);
			assert(max_index > shm_index);
			assert(INVALID_SHMID != linkctl->rtnobj_shmid[shm_index]);
			assert(shm_off < ((off_t)1 << (MIN_RTNOBJ_SHM_INDEX + shm_index)));
			rtnobj = (rtnobj_hdr_t *)(linkctl->rtnobj_shm_base[shm_index] + shm_off);
			assert((rtnobj->objhash != objhash) || (rtnobj->objLen == objSize));
			if ((rtnobj->objLen == objSize) && (rtnobj->objhash == objhash))
				break;
			prev_rtnobj = rtnobj;
			shm_index_off = rtnobj->next_rtnobj_shm_offset;
		}
		if ((rtnobj_sm_off_t)NULL_RTNOBJ_SM_OFF_T != shm_index_off)
		{	/* If found return */
			assert(rtnobj->initialized);
			/* If gtm_autorelink_keeprtn is FALSE (i.e. we maintain reference counts even on process exit),
			 * we do not expect refcnt to go to high values as it is the # of processes concurrently running
			 * and actively using this buffer and that cannot be close to 2**31 which is the max value for refcnt.
			 */
			assert(TREF(gtm_autorelink_keeprtn) || (REFCNT_INACCURATE != rtnobj->refcnt));
			if (REFCNT_INACCURATE != rtnobj->refcnt)
				rtnobj->refcnt++;	/* increment refcnt while holding the rtnobj_lock */
			if (has_relinkctl_lock)
				rel_latch(&shm_hdr->relinkctl_latch);
			RTNOBJ_DBG(fprintf(stderr, "rtnobj_shm_malloc : rtnname = %s : objhash = 0x%llx : "		\
				"linkctl = 0x%llx : Found : refcnt = %d : elem = 0x%llx\n",				\
				relinkrec->rtnname_fixed.c, (UINTPTR_T)objhash,				\
				(UINTPTR_T)linkctl, rtnobj->refcnt,					\
				(UINTPTR_T)&rtnobj->userStorage.userStart);)
			zhent->cycle = relinkrec->cycle;	/* Update private cycle to be in sync with shared copy */
			rel_latch(&relinkrec->rtnobj_latch);
			objBuff = (sm_uc_ptr_t)&rtnobj->userStorage.userStart;
			return objBuff;	/* object code starts after rtnobj_hdr_t */
		}
		assert(MAXUINT4 >= (objSize + OFFSETOF(rtnobj_hdr_t, userStorage)));
		assert(4 <= SIZEOF(tSize));	/* above assert guarantees 4-bytes is enough for storage */
		tSize = objSize + OFFSETOF(rtnobj_hdr_t, userStorage);
		sizeIndex = ceil_log2_32bit(tSize);
		assert(MAX_RTNOBJ_SIZE_BITS >= sizeIndex);
		sizeIndex = MAX(sizeIndex, MIN_RTNOBJ_SIZE_BITS);	/* Round up to minimum supported obj file size */
		/* Find the smallest shm index that can possibly contain space for sizeIndex */
		shm_index = (MIN_RTNOBJ_SHM_INDEX > sizeIndex) ? 0 : sizeIndex - MIN_RTNOBJ_SHM_INDEX;
		sizeIndex -= MIN_RTNOBJ_SIZE_BITS;
		shm_index = MAX(shm_index, shm_hdr->min_shm_index);
		/* If not found, allocate space in shared memory for one and return. Now that we are going to
		 * play with the buddy list structures in shared memory, get an overarching lock on all rtnobj shmids
		 * i.e. a lock on the entire relinkctl file
		 */
		if (!has_relinkctl_lock)
		{
			if (!grab_latch(&shm_hdr->relinkctl_latch, RLNKSHM_LATCH_TIMEOUT_SEC))
			{
				assert(FALSE);
				rel_latch(&relinkrec->rtnobj_latch);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4)
						ERR_RLNKSHMLATCH, 2, RTS_ERROR_MSTR(&linkctl->zro_entry_name));
			}
			has_relinkctl_lock = TRUE;
		}
		/* Sync up with rtnobj shmids one final time if needed now that we have the relinkctl file lock */
		SYNC_RTNOBJ_SHMID_CYCLE_IF_NEEDED(linkctl, min_index, max_index, shm_hdr, has_relinkctl_lock, relinkrec);
		/* Find a shared memory buffer to hold the routine object */
		rtnobj = NULL;
		for ( ; shm_index < max_index; shm_index++)
		{
			rtnobj_shm_hdr = &shm_hdr->rtnobj_shmhdr[shm_index];
			if (INVALID_SHMID == rtnobj_shm_hdr->rtnobj_shmid)
				continue;
			min_free_index = rtnobj_shm_hdr->rtnobj_min_free_index;
			max_free_index = rtnobj_shm_hdr->rtnobj_max_free_index;
			shm_base = linkctl->rtnobj_shm_base[shm_index];
			objIndex = MAX(sizeIndex, min_free_index);
			freeList = &rtnobj_shm_hdr->freeList[objIndex];
			for ( ; objIndex < max_free_index; objIndex++, freeList++)
			{
				assert(objIndex < NUM_RTNOBJ_SIZE_BITS);
				if (NULL_RTNOBJ_SM_OFF_T == freeList->fl)
				{
					assert(NULL_RTNOBJ_SM_OFF_T == freeList->bl);
					continue;
				}
				rtnobj = remqh_rtnobj(freeList, shm_base);
				if (NULL_RTNOBJ_SM_OFF_T == freeList->fl)
				{
					assert(NULL_RTNOBJ_SM_OFF_T == freeList->bl);
					/* This was the only element in the freeList.
					 * Set min_free_index/max_free_index to impossible values
					 * if the new minimums/maximums need to be recomputed.
					 */
					if (objIndex == min_free_index)
						min_free_index = NUM_RTNOBJ_SIZE_BITS;
					if (max_free_index == (objIndex + 1))
						max_free_index = 0;
				} else if (min_free_index > sizeIndex)
				{
					assert(NUM_RTNOBJ_SIZE_BITS > min_free_index);
					min_free_index = NUM_RTNOBJ_SIZE_BITS;
				}
				break;
			}
			if (NULL != rtnobj)
				break;
		}
		if (NULL == rtnobj)
		{	/* Need to allocate rtnobj buffer in a new shared memory segment */
			assert(shm_index >= max_index);
			shm_size = ((size_t)1 << (shm_index + MIN_RTNOBJ_SHM_INDEX));
			ADJUST_SHM_SIZE_FOR_HUGEPAGES(shm_size, shm_size); /* second parameter "shm_size" is adjusted size */
			/* If minimum huge page size is much higher than requested shm size, adjust "shm_index"
			 * accordingly so we use the allocated (bigger-than-requested) shm completely.
			 */
			shmSizeIndex = ceil_log2_64bit(shm_size);
			assert(((size_t)1 << shmSizeIndex) == shm_size);
			assert(shmSizeIndex >= (shm_index + MIN_RTNOBJ_SHM_INDEX));
			shm_index = shmSizeIndex - MIN_RTNOBJ_SHM_INDEX;
			shmid = shmget(IPC_PRIVATE, shm_size, RWDALL | IPC_CREAT);
			if (-1 == shmid)
			{
				save_errno = errno;
				rel_latch(&shm_hdr->relinkctl_latch);
				rel_latch(&relinkrec->rtnobj_latch);
				SNPRINTF(errstr, SIZEOF(errstr), "rtnobj shmget() failed for shmsize=0x%llx", shm_size);
				ISSUE_RELINKCTLERR_SYSCALL(&linkctl->zro_entry_name, errstr, save_errno);
			}
			STAT_FILE(linkctl->zro_entry_name.addr, &dir_stat_buf, stat_res);
			if (-1 == stat_res)
			{
				save_errno = errno;
				rel_latch(&shm_hdr->relinkctl_latch);
				rel_latch(&relinkrec->rtnobj_latch);
				shm_rmid(shmid);	/* if error removing shmid we created, just move on */
				SNPRINTF(errstr, SIZEOF(errstr), "rtnobj stat() of file %s failed", linkctl->zro_entry_name.addr);
				ISSUE_RELINKCTLERR_SYSCALL(&linkctl->zro_entry_name, errstr, save_errno);
			}
			if (!gtm_permissions(&dir_stat_buf, &user_id, &group_id, &perm, PERM_IPC|PERM_EXEC, &pdd))
			{
				rel_latch(&shm_hdr->relinkctl_latch);
				rel_latch(&relinkrec->rtnobj_latch);
				shm_rmid(shmid);	/* if error removing shmid we created, just move on */
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(10 + PERMGENDIAG_ARG_COUNT)
						ERR_RELINKCTLERR, 2, RTS_ERROR_MSTR(&linkctl->zro_entry_name),
						ERR_PERMGENFAIL, 4, RTS_ERROR_STRING("rtnobj"),
						RTS_ERROR_MSTR(&linkctl->zro_entry_name),
						PERMGENDIAG_ARGS(pdd));
			}
			if (-1 == shmctl(shmid, IPC_STAT, &shmstat))
			{
				save_errno = errno;
				rel_latch(&shm_hdr->relinkctl_latch);
				rel_latch(&relinkrec->rtnobj_latch);
				shm_rmid(shmid);	/* if error removing shmid we created, just move on */
				SNPRINTF(errstr, SIZEOF(errstr), "rtnobj shmctl(IPC_STAT) failed for shmid=%d shmsize=0x%llx",
					shmid, shm_size);
				ISSUE_RELINKCTLERR_SYSCALL(&linkctl->zro_entry_name, errstr, save_errno);
			}
			/* change uid, group-id and permissions if needed */
			need_shmctl = FALSE;
			if ((INVALID_UID != user_id) && (user_id != shmstat.shm_perm.uid))
			{
				shmstat.shm_perm.uid = user_id;
				need_shmctl = TRUE;
			}
			if ((INVALID_GID != group_id) && (group_id != shmstat.shm_perm.gid))
			{
				shmstat.shm_perm.gid = group_id;
				need_shmctl = TRUE;
			}
			if (shmstat.shm_perm.mode != perm)
			{
				shmstat.shm_perm.mode = perm;
				need_shmctl = TRUE;
			}
			if (need_shmctl && (-1 == shmctl(shmid, IPC_SET, &shmstat)))
			{
				save_errno = errno;
				rel_latch(&shm_hdr->relinkctl_latch);
				rel_latch(&relinkrec->rtnobj_latch);
				shm_rmid(shmid);	/* if error removing shmid we created, just move on */
				SNPRINTF(errstr, SIZEOF(errstr), "rtnobj shmctl(IPC_SET) failed for shmid=%d shmsize=0x%llx",
					shmid, shm_size);
				ISSUE_RELINKCTLERR_SYSCALL(&linkctl->zro_entry_name, errstr, save_errno);
			}
			shm_base = (sm_uc_ptr_t)do_shmat_exec_perm(shmid, shm_size, &save_errno);
			if (-1 == (sm_long_t)shm_base)
			{
				save_errno = errno;
				rel_latch(&shm_hdr->relinkctl_latch);
				rel_latch(&relinkrec->rtnobj_latch);
				shm_rmid(shmid);	/* if error removing shmid we created, just move on */
				SNPRINTF(errstr, SIZEOF(errstr), "rtnobj shmat() failed for shmid=%d shmsize=0x%llx",
					shmid, shm_size);
				ISSUE_RELINKCTLERR_SYSCALL(&linkctl->zro_entry_name, errstr, save_errno);
			}
			assert(shm_index < NUM_RTNOBJ_SHM_INDEX);
			rtnobj_shm_hdr = &shm_hdr->rtnobj_shmhdr[shm_index];
			objIndex = (shm_index + MIN_RTNOBJ_SHM_INDEX - MIN_RTNOBJ_SIZE_BITS);
			assert(objIndex < NUM_RTNOBJ_SIZE_BITS);
			freeList = &rtnobj_shm_hdr->freeList[objIndex];
			rtnobj = (rtnobj_hdr_t *)shm_base;
			assert(NULL_RTNOBJ_SM_OFF_T == freeList->fl);
			assert(NULL_RTNOBJ_SM_OFF_T == freeList->bl);
			assert(sizeIndex <= objIndex);
			if (sizeIndex < objIndex)
			{	/* rtnobj_shm_hdr->rtnobj_min_free_index and ->rtnobj_min_free_index will be set a little later */
				min_free_index = NUM_RTNOBJ_SIZE_BITS;
				max_free_index = 0;
			} else
			{	/* rtnobj_shm_hdr->rtnobj_min_free_index and ->rtnobj_min_free_index are already set correctly.
				 * set min_free_index/max_free_index such that the above two dont get set again (a little later).
				 */
				assert(NUM_RTNOBJ_SIZE_BITS == rtnobj_shm_hdr->rtnobj_min_free_index);
				assert(0 == rtnobj_shm_hdr->rtnobj_max_free_index);
				assert(NUM_RTNOBJ_SIZE_BITS != objIndex);
				min_free_index = objIndex;
				assert(0 != NUM_RTNOBJ_SIZE_BITS);
				max_free_index = NUM_RTNOBJ_SIZE_BITS;
			}
			rtnobj_shm_hdr->rtnobj_shmid = shmid;
			assert(0 == rtnobj_shm_hdr->real_len);
			rtnobj_shm_hdr->shm_len = shm_size;
			if (shm_hdr->rtnobj_min_shm_index > shm_index)
			{
				assert(NUM_RTNOBJ_SHM_INDEX == shm_hdr->rtnobj_min_shm_index);
				shm_hdr->rtnobj_min_shm_index = shm_index;
			}
			assert(linkctl->rtnobj_max_shm_index == shm_hdr->rtnobj_max_shm_index);
			assert(shm_hdr->rtnobj_max_shm_index < (shm_index + 1));
			SHM_WRITE_MEMORY_BARRIER; /* Update everything except "rtnobj_max_shm_index" BEFORE write memory barrier */
			shm_hdr->rtnobj_max_shm_index = (shm_index + 1);
			/* Sync shared memory with private contents for this process to avoid duplicate shmget/shmat */
			linkctl->rtnobj_min_shm_index = shm_hdr->rtnobj_min_shm_index;
			linkctl->rtnobj_max_shm_index = shm_hdr->rtnobj_max_shm_index;
			linkctl->rtnobj_shm_base[shm_index] = shm_base;
			linkctl->rtnobj_shmid[shm_index] = shmid;
		}
		/* At this point, we have a non-null rtnobj but it could be bigger than we want.
		 * Split it into half until we reach the desired size. Insert into free queues along the way.
		 */
		assert(objIndex >= sizeIndex);
		if (objIndex > sizeIndex)
		{
			if (0 == max_free_index)
			{
				rtnobj_shm_hdr->rtnobj_max_free_index = objIndex;
				max_free_index = objIndex;
			}
			rtnobjSize = ((off_t)1 << (objIndex + MIN_RTNOBJ_SIZE_BITS));
			freeList = &rtnobj_shm_hdr->freeList[objIndex];
			do
			{
				rtnobjSize /= 2;
				objIndex--;
				freeList--;
				rtnobj2 = (rtnobj_hdr_t *)((sm_uc_ptr_t)rtnobj + rtnobjSize);
				rtnobj2->state = STATE_FREE;
				rtnobj2->queueIndex = objIndex;
				DEBUG_ONLY(shm_off = (sm_uc_ptr_t)rtnobj2 - shm_base);
				assert(0 == (shm_off % ((sm_off_t)1 << (objIndex + MIN_RTNOBJ_SIZE_BITS))));
				/* The following fields need initialization only when state becomes STATE_ALLOCATED.
				 *	rtnobj2->initialized
				 *	rtnobj2->refcnt
				 *	rtnobj2->src_cksum_8byte
				 *	rtnobj2->next_rtnobj_shm_offset
				 *	rtnobj2->relinkctl_index
				 *	rtnobj2->objLen
				 */
				insqt_rtnobj(&rtnobj2->userStorage.freePtr, freeList, shm_base);
			} while (objIndex > sizeIndex);
			if (NUM_RTNOBJ_SIZE_BITS == min_free_index)
				rtnobj_shm_hdr->rtnobj_min_free_index = sizeIndex;
		} else
		{
			if (NUM_RTNOBJ_SIZE_BITS == min_free_index)
			{
				assert(objIndex == rtnobj_shm_hdr->rtnobj_min_free_index);
				if (0 == max_free_index)
				{	/* Need to recompute BOTH rtnobj_shm_hdr->rtnobj_min_free_index
					 * AND rtnobj_shm_hdr->rtnobj_max_free_index. But the fact that
					 * min and max are the same implies there are NO more free buffers
					 * in this entire shared memory so the recomputation is easy.
					 */
					assert((objIndex + 1) == rtnobj_shm_hdr->rtnobj_max_free_index);
					rtnobj_shm_hdr->rtnobj_max_free_index = 0;
					rtnobj_shm_hdr->rtnobj_min_free_index = NUM_RTNOBJ_SIZE_BITS;
				} else
				{	/* Need to ONLY recompute rtnobj_shm_hdr->rtnobj_min_free_index */
					max_free_index = rtnobj_shm_hdr->rtnobj_max_free_index;
					freeList = &rtnobj_shm_hdr->freeList[objIndex];
					for ( ; objIndex < max_free_index; objIndex++, freeList++)
					{
						assert(objIndex < NUM_RTNOBJ_SIZE_BITS);
						if (NULL_RTNOBJ_SM_OFF_T != freeList->fl)
						{
							rtnobj_shm_hdr->rtnobj_min_free_index = objIndex;
							break;
						}
					}
					assert(objIndex < max_free_index);
				}
			} else if (0 == max_free_index)
			{	/* Need to ONLY recompute rtnobj_shm_hdr->rtnobj_min_free_index */
				assert((objIndex + 1) == rtnobj_shm_hdr->rtnobj_max_free_index);
				min_free_index = rtnobj_shm_hdr->rtnobj_min_free_index;
				freeList = &rtnobj_shm_hdr->freeList[objIndex];
				for ( ; objIndex >= min_free_index; objIndex--, freeList--)
				{
					assert(objIndex < NUM_RTNOBJ_SIZE_BITS);
					if (NULL_RTNOBJ_SM_OFF_T != freeList->fl)
					{
						rtnobj_shm_hdr->rtnobj_max_free_index = objIndex + 1;
						break;
					}
				}
				assert(objIndex >= min_free_index);
			}
		}
		rtnobj->state = STATE_ALLOCATED;
		rtnobj->queueIndex = sizeIndex;
		rtnobj_shm_hdr->real_len += objSize;
		elemSize = ((gtm_uint64_t)1 << (sizeIndex + MIN_RTNOBJ_SIZE_BITS));
		rtnobj_shm_hdr->used_len += elemSize;
		DEBUG_ONLY(rtnobj_verify_min_max_free_index(rtnobj_shm_hdr));
		DEBUG_ONLY(rtnobj_verify_freelist_fl_bl(rtnobj_shm_hdr, shm_base);)
		/* Now that we have gotten a "rtnobj" pointer, we can release the relinkctl latch on shared memory.
		 * We still need to hold on to the lower-granular relinkrec->rtnobj_latch to read the object file
		 * into shared memory.
		 */
		rel_latch(&shm_hdr->relinkctl_latch);
		assert(((off_t)1 << (sizeIndex + MIN_RTNOBJ_SIZE_BITS)) >= (objSize + OFFSETOF(rtnobj_hdr_t, userStorage)));
		assert(((off_t)1 << (sizeIndex + MIN_RTNOBJ_SIZE_BITS - 1)) < (objSize + OFFSETOF(rtnobj_hdr_t, userStorage)));
		shm_off = (sm_uc_ptr_t)rtnobj - shm_base;
		assert(0 == (shm_off % ((sm_off_t)1 << (sizeIndex + MIN_RTNOBJ_SIZE_BITS))));
		rtnobj->initialized = TRUE;
		rtnobj->refcnt = 1;
		assert(0 <= shm_off);
		assert(shm_off < rtnobj_shm_hdr->shm_len);
		shm_index_off = RTNOBJ_SET_SHM_INDEX_OFF(shm_index, shm_off);
		if (NULL != prev_rtnobj)
			prev_rtnobj->next_rtnobj_shm_offset = shm_index_off;
		else
			relinkrec->rtnobj_shm_offset = shm_index_off;
		rtnobj->next_rtnobj_shm_offset = (rtnobj_sm_off_t)NULL_RTNOBJ_SM_OFF_T;
		rtnobj->relinkctl_index = relinkrec - linkctl->rec_base;
		rtnobj->objLen = objSize;
		rtnobj->objhash = objhash;
		relinkrec->objLen += objSize;
		relinkrec->usedLen += elemSize;
		relinkrec->numvers++;
		/* Fill the shared memory buffer with the object code */
		objBuff = (sm_uc_ptr_t)&rtnobj->userStorage.userStart;
		if (-1 == lseek(fd, NATIVE_HDR_LEN, SEEK_SET))
			return_NULL = TRUE;
		else
		{
			DOREADRL(fd, objBuff, objSize, actualSize);
			return_NULL = (actualSize != objSize);
		}
		if (return_NULL)
		{
			/* Free allocated object in shared memory using rtnobj_shm_free. Set up a dummy rtnhdr
			 * with just the necessary fields initialized since rtnobj_shm_free requires it.
			 */
			rhdr = &tmprhd;
			rhdr->shared_ptext_adr = objBuff + SIZEOF(rhdtyp);
			rhdr->zhist = zhist;
			DEBUG_ONLY(rhdr->shared_object = TRUE);	/* for an assert inside rtnobj_shm_free */
			DEBUG_ONLY(rhdr->relinkctl_bkptr = linkctl);	/* for an assert inside rtnobj_shm_free */
			/* We already hold the relinkrec->rtnobj_latch. Free up the shared object before releasing this
			 * lock to avoid other processes from seeing the allocated shared object and assuming it is
			 * properly initialized when actually it is not. rtnobj_shm_free obtains the relinkrec->rtnobj_latch
			 * which would fail an assert since we already hold that latch so add a dbg-flag to avoid an assert.
			 */
			rtnobj_shm_free(rhdr, LATCH_GRABBED_TRUE); /* Note: this will release relinkrec->rtnobj_latch for us */
			return NULL;	/* caller will issue error */
		}
		rhdr = (rhdtyp *)objBuff;
		codelen = rhdr->ptext_end_adr - rhdr->ptext_adr;	/* Length of object code */
		assert(0 < codelen);
		codeadr = objBuff + (UINTPTR_T)rhdr->ptext_adr;		/* Pointer to object code */
		cacheflush(codeadr, codelen, BCACHE);			/* Cacheflush executable part from instruction cache */
		RTNOBJ_DBG(fprintf(stderr, "rtnobj_shm_malloc : rtnname = %s : objhash = 0x%llx : "		\
			"linkctl = 0x%llx : Inserted : refcnt = %d : elem = 0x%llx : numvers = %d\n",		\
			relinkrec->rtnname_fixed.c, (UINTPTR_T)objhash,				\
			(UINTPTR_T)linkctl, rtnobj->refcnt,					\
			(UINTPTR_T)objBuff, relinkrec->numvers);)
		relinkrec->objhash = objhash;
		++relinkrec->cycle;	/* cycle bump is because of the implicit ZRUPDATE that linking a routine does */
		if (0 == relinkrec->cycle)
			relinkrec->cycle = 1;	/* cycle value of 0 has special meaning, so bump to 1 in case we roll over */
		zhent->cycle = relinkrec->cycle;	/* Update private cycle to be in sync with shared copy */
		rel_latch(&relinkrec->rtnobj_latch);
		return objBuff;
	} while (TRUE);
}

/* Function to free the object file in rtnobj shared memory whose routine header is "rhead". If this object file is used
 * by more than one process (i.e. reference count is > 1), then decrement the reference count and return. This fast path
 * only holds lock on the relink record corresponding to this object's routine-name. If, however, the reference count is 1,
 * space for this object file will be freed up and that requires holding a lock on the entire relinkctl shared memory (and
 * effectively all rtnobj shared memory segments).
 */
void	rtnobj_shm_free(rhdtyp *rhead, boolean_t latch_grabbed)
{
	relinkrec_t		*relinkrec;
	open_relinkctl_sgm	*linkctl;
	relinkshm_hdr_t		*shm_hdr;
	int			maxObjIndex, sizeIndex;
	int			min_index, max_index, min_free_index, max_free_index, shm_index;
	gtm_uint64_t		elemSize, origElemSize, objLen;
	rtnobj_hdr_t		*rtnobj, *prev_rtnobj, *rtnobj2;
	rtnobj_sm_off_t		shm_index_off;
	rtnobjshm_hdr_t		*rtnobj_shm_hdr;
	sm_uc_ptr_t		shm_base;
	sm_off_t		shm_off, rtnobj_off;
	que_ent_ptr_t		freeList;
	zro_validation_entry	*zhent;
	sm_uc_ptr_t		objBuff;
	int			maxvers, curvers;
	DEBUG_ONLY(size_t	shm_size;)
	DEBUG_ONLY(int		dbg_shm_index;)
#	ifdef DEBUG
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	endif
	/* Note that if breakpoints are in effect, rhead->shared_ptext_adr will not be equal to rhead->ptext_adr.
	 * This is because we would have taken a private copy of the code (for breakpoints) into ptext_adr and kept
	 * shared_ptext_adr untouched. In that case, it is still possible this function is called as part of process
	 * exit (when breakpoints on a shared routine object are still active). We will still need to free-up/decrement-refcnt
	 * the shared copy (shared_ptext_adr). So look at shared_ptext_adr below instead of ptext_adr;
	 */
	assert(rhead->shared_object);
	assert(NULL != rhead->shared_ptext_adr);
	if (NULL == rhead->shared_ptext_adr)
		return;	/* in pro, be safe */
	assert(process_exiting || (rhead->shared_ptext_adr == rhead->ptext_adr));
	objBuff = rhead->shared_ptext_adr - SIZEOF(rhdtyp);
	rtnobj = (rtnobj_hdr_t *)(objBuff - OFFSETOF(rtnobj_hdr_t, userStorage));
	assert(STATE_ALLOCATED == rtnobj->state);
	assert(rtnobj->initialized);
	assert(0 < rtnobj->refcnt);
	assert(NULL != rhead->zhist);
	zhent = rhead->zhist->end - 1;
	relinkrec = zhent->relinkrec;
	linkctl = zhent->relinkctl_bkptr;
	assert(rhead->relinkctl_bkptr == linkctl);
	shm_hdr = GET_RELINK_SHM_HDR(linkctl);
	assert(!linkctl->locked);
	assert(!linkctl->hdr->file_deleted);
	sizeIndex = rtnobj->queueIndex;
#	ifdef DEBUG
	/* Find shm_index corresponding to this routine buffer to cross check later */
	min_index = linkctl->rtnobj_min_shm_index;
	max_index = linkctl->rtnobj_max_shm_index;
	for (dbg_shm_index = min_index; dbg_shm_index < max_index; dbg_shm_index++)
	{
		rtnobj_shm_hdr = &shm_hdr->rtnobj_shmhdr[dbg_shm_index];
		if (INVALID_SHMID == rtnobj_shm_hdr->rtnobj_shmid)
			continue;
		shm_base = linkctl->rtnobj_shm_base[dbg_shm_index];
		assert(NULL != shm_base);
		shm_size = ((size_t)1 << (dbg_shm_index + MIN_RTNOBJ_SHM_INDEX));
		if (((gtm_uint64_t)shm_base <= (gtm_uint64_t)rtnobj)
				&& (((gtm_uint64_t)shm_base + shm_size) > (gtm_uint64_t)rtnobj))
			break;	/* shared memory corresponding to rtnobj is found */
	}
	assert(dbg_shm_index < max_index);
#	endif
	if (!latch_grabbed && !grab_latch(&relinkrec->rtnobj_latch, RLNKREC_LATCH_TIMEOUT_SEC))
	{
		assert(FALSE);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5)
				ERR_RLNKRECLATCH, 3, relinkrec->rtnname_fixed.c, RTS_ERROR_MSTR(&linkctl->zro_entry_name));
	}
	assert(0 < rtnobj->refcnt);
	assert(TREF(gtm_autorelink_keeprtn) || (REFCNT_INACCURATE != rtnobj->refcnt));
	if (REFCNT_INACCURATE != rtnobj->refcnt)
		rtnobj->refcnt--;	/* decrement refcnt while holding the rtnobj_lock */
	if (rtnobj->refcnt)
	{	/* The loaded object cannot be freed until refcnt becomes 0. But caller's job done. Return. */
		RTNOBJ_DBG(fprintf(stderr,											\
			"rtnobj_shm_free : rtnname = %s : objhash = 0x%llx : Decremented : refcnt = %d : elem = 0x%llx\n",	\
			relinkrec->rtnname_fixed.c, (UINTPTR_T)rtnobj->objhash, rtnobj->refcnt,			\
			(UINTPTR_T)objBuff);)
		rel_latch(&relinkrec->rtnobj_latch);
		return;
	}
	assert(sizeIndex == rtnobj->queueIndex); /* since this pid has not freed it yet, queueIndex must not change concurrently */
	/* Note: Although we hold the rtnobj_latch, it is possible that new rtnobj_shmids get created concurrently
	 * and an addition happens to the rtnobj->next_rtnobj_shm_offset linked list pointing to the new shmids.
	 * But since additions to the linked list happen only at the end, we are guaranteed that the element of
	 * interest (rtnobj which we want to free and have already located in rtnobj_shm_malloc) would be found
	 * without reaching the tail portion of that linked list (which would require attaching to the new shmid).
	 * So no need to invoke the SYNC_RTNOBJ_SHMID_CYCLE_IF_NEEDED macro like we needed to in "rtnobj_shm_malloc".
	 */
	shm_index_off = relinkrec->rtnobj_shm_offset;
	prev_rtnobj = NULL;
	maxvers = relinkrec->numvers;
	curvers = 0;
	while ((rtnobj_sm_off_t)NULL_RTNOBJ_SM_OFF_T != shm_index_off)
	{
		assertpro(maxvers >= curvers++);	/* assertpro to avoid infinite loops in PRO (just in case) */
		shm_index = RTNOBJ_GET_SHM_INDEX(shm_index_off);
		shm_off = RTNOBJ_GET_SHM_OFFSET(shm_index_off);
		assert(0 == (shm_off % 8));
		/* Since we hold the rtnobj_latch we should see valid values of shm_index/shm_off */
		assert(min_index <= shm_index);
		assert(max_index > shm_index);
		assert(INVALID_SHMID != linkctl->rtnobj_shmid[shm_index]);
		assert(shm_off < ((off_t)1 << (MIN_RTNOBJ_SHM_INDEX + shm_index)));
		rtnobj2 = (rtnobj_hdr_t *)(linkctl->rtnobj_shm_base[shm_index] + shm_off);
		if (rtnobj2 == rtnobj)
			break;
		prev_rtnobj = rtnobj2;
		shm_index_off = rtnobj2->next_rtnobj_shm_offset;
	}
	assertpro((rtnobj_sm_off_t)NULL_RTNOBJ_SM_OFF_T != shm_index_off);
	shm_index_off = rtnobj->next_rtnobj_shm_offset;
	assert(dbg_shm_index == shm_index);
	rtnobj_shm_hdr = &shm_hdr->rtnobj_shmhdr[shm_index];
	shm_base = linkctl->rtnobj_shm_base[shm_index];
	maxObjIndex = (shm_index + MIN_RTNOBJ_SHM_INDEX - MIN_RTNOBJ_SIZE_BITS);
	assert(sizeIndex <= maxObjIndex);
	elemSize = origElemSize = ((gtm_uint64_t)1 << (sizeIndex + MIN_RTNOBJ_SIZE_BITS));
	/* Now that we need to unload the object from shared memory, get a shared memory lock */
	if (!grab_latch(&shm_hdr->relinkctl_latch, RLNKSHM_LATCH_TIMEOUT_SEC))
	{
		assert(FALSE);
		rel_latch(&relinkrec->rtnobj_latch);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_RLNKSHMLATCH, 2, RTS_ERROR_MSTR(&linkctl->zro_entry_name));
	}
	assert(rtnobj->objLen <= rtnobj_shm_hdr->real_len);
	assert(elemSize <= rtnobj_shm_hdr->used_len);
	objLen = rtnobj->objLen;
	rtnobj_shm_hdr->real_len -= objLen;
	rtnobj_shm_hdr->used_len -= elemSize;
	/* Check if buddy of "rtnobj" is free and if so free the two together. Merge the two into a bigger chunk
	 * and redo the free-buddy-check/merge as much as possible.
	 */
	min_free_index = rtnobj_shm_hdr->rtnobj_min_free_index;
	if (sizeIndex < min_free_index)
	{
		min_free_index = sizeIndex;
		rtnobj_shm_hdr->rtnobj_min_free_index = sizeIndex; /* we are about to do a insqt_rtnobj at freeList[sizeIndex] */
	}
	freeList = &rtnobj_shm_hdr->freeList[sizeIndex];
	rtnobj_off = (sm_uc_ptr_t)rtnobj - shm_base;
	while (sizeIndex < maxObjIndex)
	{
		rtnobj2 = (rtnobj_hdr_t *)(shm_base + (rtnobj_off ^ elemSize));	/* address of buddy */
		assert(rtnobj2->queueIndex <= sizeIndex);
		assert((STATE_ALLOCATED == rtnobj2->state) || (STATE_FREE == rtnobj2->state));
		if (STATE_ALLOCATED == rtnobj2->state)
			break;
		if (rtnobj2->queueIndex != sizeIndex)
			break;
		remq_rtnobj_specific(freeList, shm_base, rtnobj2);
		if ((NULL_RTNOBJ_SM_OFF_T == freeList->fl) && (sizeIndex == min_free_index))
			min_free_index = NUM_RTNOBJ_SIZE_BITS;
		if (rtnobj2 < rtnobj)
		{
			assert(elemSize == ((sm_off_t)rtnobj - (sm_off_t)rtnobj2));
			rtnobj = rtnobj2;	/* Pick lower address buddy for top of new bigger block */
			assert(rtnobj_off >= (sm_off_t)elemSize);
			rtnobj_off -= elemSize;
		}
		freeList++;
		sizeIndex++;
		elemSize *= 2;
	}
	rtnobj->state = STATE_FREE;
	rtnobj->queueIndex = sizeIndex;
	assert(rtnobj_off == ((sm_uc_ptr_t)rtnobj - shm_base));
	assert(0 == (rtnobj_off % ((sm_off_t)1 << (sizeIndex + MIN_RTNOBJ_SIZE_BITS))));
	insqt_rtnobj(&rtnobj->userStorage.freePtr, freeList, shm_base);
	max_free_index = rtnobj_shm_hdr->rtnobj_max_free_index;
	if (sizeIndex >= max_free_index)
		rtnobj_shm_hdr->rtnobj_max_free_index = sizeIndex + 1;	/* Recompute max_free_index */
	if (NUM_RTNOBJ_SIZE_BITS == min_free_index)
	{	/* Recompute min_free_index */
		max_free_index = sizeIndex + 1;
		sizeIndex = rtnobj_shm_hdr->rtnobj_min_free_index;
		freeList = &rtnobj_shm_hdr->freeList[sizeIndex];
		for ( ; sizeIndex < max_free_index; sizeIndex++, freeList++)
		{
			assert(sizeIndex < NUM_RTNOBJ_SIZE_BITS);
			if (NULL_RTNOBJ_SM_OFF_T != freeList->fl)
				break;
		}
		rtnobj_shm_hdr->rtnobj_min_free_index = sizeIndex;
	}
	DEBUG_ONLY(rtnobj_verify_min_max_free_index(rtnobj_shm_hdr));
	DEBUG_ONLY(rtnobj_verify_freelist_fl_bl(rtnobj_shm_hdr, shm_base);)
	rel_latch(&shm_hdr->relinkctl_latch);
	if (NULL != prev_rtnobj)
		prev_rtnobj->next_rtnobj_shm_offset = shm_index_off;
	else
		relinkrec->rtnobj_shm_offset = shm_index_off;
	assert(objLen <= relinkrec->objLen);
	relinkrec->objLen -= objLen;
	assert(origElemSize <= relinkrec->usedLen);
	relinkrec->usedLen -= origElemSize;
	assert(0 < relinkrec->numvers);
	relinkrec->numvers--;
	RTNOBJ_DBG(fprintf(stderr,												\
		"rtnobj_shm_free : rtnname = %s : objhash = 0x%llx : Freed : refcnt = %d : elem = 0x%llx : numvers = %d\n",	\
		relinkrec->rtnname_fixed.c, (UINTPTR_T)rtnobj->objhash, rtnobj->refcnt,						\
		(UINTPTR_T)objBuff, relinkrec->numvers);)
	rel_latch(&relinkrec->rtnobj_latch);
	return;
}
#endif
