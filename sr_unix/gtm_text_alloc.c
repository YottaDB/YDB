/****************************************************************
 *								*
 *	Copyright 2007, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Storage manager for mmap() allocated storage used for executable code.
 * Uses power-of-two "buddy" system as described by Knuth. Allocations up to
 * size <pagesize> - SIZEOF(header) are managed by the buddy system. Larger
 * sizes are only "tracked" and then released via munmap() when they are freed.
 *
 * The algorithms used in this module are very similar to those used in
 * gtm_malloc.c with some changes and fewer of the generation options
 * since this is a more special purpose type allocation mechanism.
 */

#include "mdef.h"

#include <sys/types.h>
#include <sys/mman.h>
#include <signal.h>
#include <stddef.h>
#include <errno.h>
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtm_stdlib.h"
#include "gtm_unistd.h"

#include "eintr_wrappers.h"
#include "mdq.h"
#include "min_max.h"
#include "error.h"
#include "gtmmsg.h"
#include "caller_id.h"
#include "gtm_text_alloc.h"
#include "gtmdbglvl.h"
#include "gtmio.h"
#include "have_crit.h"

GBLREF  int		process_exiting;		/* Process is on it's way out */
GBLREF	volatile int4	fast_lock_count;		/* Stop stale/epoch processing while we have our parts exposed */
GBLREF	uint4		gtmDebugLevel;
GBLREF	size_t		gtm_max_storalloc;		/* Max value for $ZREALSTOR or else memory error is raised */

OS_PAGE_SIZE_DECLARE

#ifdef COMP_GTA		/* Only build this routine if it is going to be called */
/* This module is built in two different ways: (1) For z/OS the allocation and free routines will just call
 * __malloc31() and free() respectively since mmap() on z/OS does not support the necessary features as of this
 * writing (12/2008). (2) For all other platforms that use this module (Linux and Tru64 builds currently), the
 * module will expand with the mmap code. [SE 12/2008]
 */

/* The MAXTWO is set to pagesize and MINTWO to 5 sizes below that. Our systems have page
 * sizes of 16K, 8K, and 4K.
 */
#define MAXTWO gtm_os_page_size
#define MINTWO TwoTable[0]		/* Computed by gtaSmInit() */
#define MAXINDEX 5

/* Fields to help instrument our algorithm */
GBLREF	size_t		totalRmalloc;			/* Total storage allocated through malloc() */
GBLREF  size_t    	totalRallocGta;                 /* Total storage currently (real) mmap alloc'd */
GBLREF  size_t     	totalAllocGta;                  /* Total mmap allocated (includes allocation overhead but not free space */
GBLREF  size_t     	totalUsedGta;                   /* Sum of "in-use" portions (totalAllocGta - overhead) */
static	int		totalAllocs;                    /* Total alloc requests */
static	int		totalFrees;                     /* Total free requests */
static	size_t		rAllocMax;                      /* Maximum value of totalRallocGta */
static	int		allocCnt[MAXINDEX + 2];         /* Alloc count satisfied by each queue size */
static	int		freeCnt[MAXINDEX + 2];          /* Free count for element in each queue size */
static	int		elemSplits[MAXINDEX + 2];       /* Times a given queue size block was split */
static	int		elemCombines[MAXINDEX + 2];     /* Times a given queue block was formed by buddies being recombined */
static	int		freeElemCnt[MAXINDEX + 2];      /* Current count of elements on the free queue */
static	int		freeElemMax[MAXINDEX + 2];      /* Maximum number of blocks on the free queue */

error_def(ERR_INVDBGLVL);
error_def(ERR_MEMORY);
error_def(ERR_MEMORYRECURSIVE);
error_def(ERR_SYSCALL);
error_def(ERR_TEXT);
error_def(ERR_TRNLOGFAIL);

#define INCR_CNTR(x) ++x
#define DECR_CNTR(x) --x
#define INCR_SUM(x, y) x += y
#define DECR_SUM(x, y) {x -= y; assert(0 <= x);}
#define SET_MAX(max, tst) max = MAX(max, tst)
#define SET_ELEM_MAX(idx) SET_MAX(freeElemMax[idx], freeElemCnt[idx])
#define CALLERID ((unsigned char *)caller_id())
#ifdef DEBUG
#  define TRACE_TXTALLOC(addr,len) {if (GDL_SmTrace & gtmDebugLevel) \
 			FPRINTF(stderr, "TxtAlloc at 0x"lvaddr" of %ld bytes from 0x"lvaddr"\n", addr, len, CALLERID);}
#  define TRACE_TXTFREE(addr,len)   {if (GDL_SmTrace & gtmDebugLevel) \
			FPRINTF(stderr, "TxtFree at 0x"lvaddr" of %ld bytes from 0x"lvaddr"\n", addr, len, CALLERID);}
#else
#  define TRACE_TXTALLOC(addr, len)
#  define TRACE_TXTFREE(addr, len)
#endif

#ifdef __MVS__

static  uint4  		TwoTable[MAXINDEX + 2];
/* ******* z/OS expansion ******* */
#undef malloc
#undef free
#include <rtnhdr.h>
#include "obj_file.h"

/* This function is meant as a temporary replacement for the gtm_text_alloc code that uses mmap.
 * ABS 2008/12 - It is deficient in two regards:
 * 1) It abuses textElem - the abuse stems from account needs.  It was hoped that we could simply
 * abuse textElem to hold the actual length of memory allocated and then use the size of textElem
 * as the offset to the original start of memory address that was malloc'ed.  However, the
 * userStart of memory needs to be SECTION_ALIGN_BOUNDARY byte aligned.
 *	action: don't use textElem
 * 2) SECTION_ALIGN_BOUNDARY is 16 bytes in 64bit world.  Since __malloc31 is returning 8 byte
 * aligned memory, we really only needed a pad of 8 bytes.  But that left no real mechanism to
 * return to the original start of memory. So we have a pad of 24 bytes.  The first 8 bytes point
 * back to the start of memory.  If the next 8 bytes are 16 byte aligned that is returned to the
 * caller.  If not, then we store the start of memory there and return the next 8 bytes.  This allows
 * us to free() the correct address.
 *	action: remove SECTION_ALIGN_BOUNDARY as a restriction for all 64bit platforms except IA64
 */
void *gtm_text_alloc(size_t size)
{
	textElem	*uStor;
	unsigned long	*aligned, *memStart;
	int		hdrSize, tSize, save_errno;

	hdrSize = SIZEOF(textElem);
	/* Pad the memory area for SECTION_ALIGN_BOUNDARY alignment required by comp_indr() */
	tSize = (int)size + hdrSize + (SECTION_ALIGN_BOUNDARY * 2);
	uStor = __malloc31(tSize);
	if (NULL != uStor)
	{
		assert(((long)uStor & (long)-8) == (long)uStor);

		aligned = (unsigned long *)&uStor->userStorage.userStart;
		aligned++;
		/* Matching the alignment as required in comp_indr() */
		aligned = (unsigned long *)ROUND_UP2((unsigned long)aligned, (unsigned long)SECTION_ALIGN_BOUNDARY);
		memStart = aligned - 1;
		*memStart = (unsigned long) uStor;
		assert((unsigned long)uStor == *memStart);
		uStor->realLen = tSize;
		INCR_SUM(totalRallocGta, tSize);
		INCR_SUM(totalAllocGta, tSize);
		INCR_SUM(totalUsedGta, tSize);
		INCR_CNTR(totalAllocs);
		SET_MAX(rAllocMax, totalRallocGta);
		TRACE_TXTALLOC(aligned, tSize);
		return (void *)aligned;
	}
	save_errno = errno;
        if (ENOMEM == save_errno)
	{
		assert(FALSE);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_MEMORY, 2, tSize, CALLERID, save_errno);
	}
	/* On non-allocate related error, give more general error and GTMASSERT */
	gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(14) ERR_SYSCALL, 5, LEN_AND_LIT("gtm_text_alloc()"), CALLFROM,
		       save_errno, 0, ERR_TEXT, 3, LEN_AND_LIT("Storage call made from"), CALLERID);
	GTMASSERT;
}

void gtm_text_free(void *addr)
{
	int		size;
	long		*storage;
	textElem	*uStor;

	storage = (long *)addr;
	storage--;
	uStor = (textElem *)*storage;
	size = uStor->realLen;

	free(uStor);
	DECR_SUM(totalRallocGta, size);
	DECR_SUM(totalAllocGta, size);
	DECR_SUM(totalUsedGta, size);
	INCR_CNTR(totalFrees);
	TRACE_TXTFREE(addr, size);
}

#else /* if not __MVS__ */

/* ******* Normal mmap() expansion ******* */

/* These routines for Unix are NOT thread or interrupt safe */
#  define TEXT_ALLOC(rsize, addr)										\
{														\
	int	save_errno;											\
	if ((0 < gtm_max_storalloc) && ((rsize + totalRmalloc + totalRallocGta) > gtm_max_storalloc))		\
	{	/* Boundary check for $gtm_max_storalloc (if set) */						\
		--gtaSmDepth;											\
		--fast_lock_count;										\
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_MEMORY, 2, rsize, CALLERID, ERR_MALLOCMAXUNIX);	\
	}													\
 	addr = mmap(NULL, rsize, (PROT_READ + PROT_WRITE + PROT_EXEC), (MAP_PRIVATE + MAP_ANONYMOUS), -1, 0);	\
	if (MAP_FAILED == addr)											\
	{													\
		--gtaSmDepth;											\
                --fast_lock_count;										\
		save_errno = errno;										\
	        if (ENOMEM == save_errno)									\
		{												\
			assert(FALSE);										\
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_MEMORY, 2, rsize, CALLERID, save_errno);  	\
		}												\
		/* On non-allocate related error, give more general error and GTMASSERT */			\
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(14) ERR_SYSCALL, 5, LEN_AND_LIT("mmap()"), CALLFROM,	\
			       save_errno, 0, ERR_CALLERID, 3, LEN_AND_LIT("TEXT_ALLOC"), CALLERID);		\
		GTMASSERT;											\
	}													\
}
#define TEXT_FREE(addr, rsize) 											\
{														\
	int	rc, save_errno;											\
	rc = munmap(addr, rsize);										\
	if (-1 == rc)												\
	{													\
		--gtaSmDepth;											\
                --fast_lock_count;										\
		save_errno = errno;										\
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(14) ERR_SYSCALL, 5, LEN_AND_LIT("munmap"), CALLFROM,	\
			       save_errno, 0, ERR_CALLERID, 3, LEN_AND_LIT("TEXT_FREE"), CALLERID);		\
		GTMASSERT;											\
	}													\
}
#define STE_FP(p) p->userStorage.links.fPtr
#define STE_BP(p) p->userStorage.links.bPtr

/* Following are values used in queueIndex in a storage element. Note that both
 * values must be less than zero for the current code to function correctly.
 */
#define QUEUE_ANCHOR		-1
#define REAL_ALLOC		-2

#ifdef DEBUG_SM
#  define DEBUGSM(x) (PRINTF x, FFLUSH(stdout))
# else
#  define DEBUGSM(x)
#endif

/* Define "routines" to enqueue and dequeue storage elements. Use define so we don't
 * have to depend on each implementation's compiler inlining to get efficient code here.
 */
#define ENQUEUE_STOR_ELEM(idx, elem)			\
{							\
	  textElem *qHdr, *fElem;			\
	  qHdr = &freeStorElemQs[idx];			\
	  STE_FP(elem) = fElem = STE_FP(qHdr);		\
	  STE_BP(elem) = qHdr;				\
	  STE_FP(qHdr) = STE_BP(fElem) = elem;		\
          INCR_CNTR(freeElemCnt[idx]);			\
          SET_ELEM_MAX(idx);				\
}

#define DEQUEUE_STOR_ELEM(elem)				\
{ 							\
	  STE_FP(STE_BP(elem)) = STE_FP(elem);		\
	  STE_BP(STE_FP(elem)) = STE_BP(elem);		\
          DECR_CNTR(freeElemCnt[elem->queueIndex]);	\
}

#define GET_QUEUED_ELEMENT(sizeIndex, uStor, qHdr)      \
{							\
	qHdr = &freeStorElemQs[sizeIndex];		\
	uStor = STE_FP(qHdr);	      			/* First element on queue */ \
	if (QUEUE_ANCHOR != uStor->queueIndex)		/* Does element exist? (Does queue point to itself?) */ \
        {						\
		DEQUEUE_STOR_ELEM(uStor);		/* It exists, dequeue it for use */ \
	} else						\
		uStor = gtaFindStorElem(sizeIndex);	\
	assert(0 == ((unsigned long)uStor & (TwoTable[sizeIndex] - 1)));	/* Verify alignment */ \
}

GBLREF readonly struct
{
	unsigned char nullHMark[4];
	unsigned char nullStr[1];
	unsigned char nullTMark[4];
} NullStruct;

static  uint4  		TwoTable[MAXINDEX + 2];
static  textElem	freeStorElemQs[MAXINDEX + 1];	/* Need full element as queue anchor for dbl-linked
							 * list since ptrs not at top of element */
static	volatile int4	gtaSmDepth;			/* If we get nested... */
static	boolean_t	gtaSmInitialized;		/* Initialized indicator */

/* Internal prototypes */
void gtaSmInit(void);
textElem *gtaFindStorElem(int sizeIndex);
int getSizeIndex(size_t size);

error_def(ERR_TRNLOGFAIL);
error_def(ERR_INVDBGLVL);
error_def(ERR_MEMORY);
error_def(ERR_SYSCALL);
error_def(ERR_MEMORYRECURSIVE);
error_def(ERR_CALLERID);
error_def(ERR_TEXT);
error_def(ERR_MALLOCMAXUNIX);

/* Initialize the storage manangement system. Things to initialize:
 *
 * - Initialize size2Index table. This table is used to convert a malloc request size
 *   to a storage queue index.
 * - Initialize queue anchor fwd/bkwd pointers to point to queue anchors so we
 *   build a circular queue. This allows elements to be added and removed without
 *   end-of-queue special casing. The queue anchor element is easily recognized because
 *   it's queue index size will be set to a special value.
 * - Initialize debug mode. See if gtm_debug_level environment variable is set and
 *   retrieve it's value if yes.
 */
void gtaSmInit(void)
{
	char		*ascNum;
	textElem	*uStor;
	int		i, sizeIndex, twoSize;

	/* WARNING!! Since this is early initialization, the assert(s) below are not well behaved if they do
	 * indeed trip. The best that can be hoped for is they give a condition handler exhausted error on
	 * GTM startup. Unfortunately, more intelligent responses are somewhat elusive since no output devices
	 * are setup nor (potentially) most of the GTM runtime.
	 */
	/* Initialize the TwoTable fields for our given page size */
	TwoTable[MAXINDEX + 1] = 0xFFFFFFFF;
	for (sizeIndex = MAXINDEX, twoSize = gtm_os_page_size; 0 <= sizeIndex; --sizeIndex, twoSize >>= 1)
	{
		assert(0 < twoSize);
		TwoTable[sizeIndex] = twoSize;
		assert(TwoTable[sizeIndex] < TwoTable[sizeIndex + 1]);
	}
	/* Need to initialize the fwd/bck ptrs in the anchors to point to themselves */
	for (uStor = &freeStorElemQs[0], i = 0; i <= MAXINDEX; ++i, ++uStor)
	{
		STE_FP(uStor) = STE_BP(uStor) = uStor;
		uStor->queueIndex = QUEUE_ANCHOR;
	}
	gtaSmInitialized = TRUE;
}

/* Recursive routine used to obtain an element on a given size queue. If no
 * elements of that size are available, we recursively call ourselves to get
 * an element of the next larger queue which we will then split in half to
 * get the one we need and place the remainder back on the free queue of its
 * new smaller size. If we run out of queues, we obtain a fresh new 'hunk' of
 * storage, carve it up into the largest block size we handle and process as
 * before.
 */
textElem *gtaFindStorElem(int sizeIndex)
{
	unsigned char	*uStorAlloc;
	textElem	*uStor, *uStor2, *qHdr;
	int		hdrSize;

	++sizeIndex;
	if (MAXINDEX >= sizeIndex)
	{	/* We have more queues to search */
	        GET_QUEUED_ELEMENT(sizeIndex, uStor, qHdr);

		/* We have a larger than necessary element now so break it in half and put
		   the second half on the queue one size smaller than us */
                INCR_CNTR(elemSplits[sizeIndex]);
		--sizeIndex;					/* Dealing now with smaller element queue */
		assert(0 <= sizeIndex && MAXINDEX >= sizeIndex);
		uStor2 = (textElem *)((unsigned long)uStor + TwoTable[sizeIndex]);
		uStor2->state = TextFree;
		uStor2->queueIndex = sizeIndex;
		assert(0 == ((unsigned long)uStor2 & (TwoTable[sizeIndex] - 1)));	/* Verify alignment */
		ENQUEUE_STOR_ELEM(sizeIndex, uStor2);		/* Place on free queue */
	} else
	{	/* Nothing left to search, [real] allocation must occur */
		TEXT_ALLOC((size_t)MAXTWO, uStorAlloc);
		uStor2 = (textElem *)uStorAlloc;
		/* Make addr "MAXTWO" byte aligned */
		uStor = (textElem *)(((unsigned long)(uStor2) + MAXTWO - 1) & (unsigned long) -MAXTWO);
                INCR_SUM(totalRallocGta, MAXTWO);
                SET_MAX(rAllocMax, totalRallocGta);
		DEBUGSM(("debuggta: Allocating block at 0x%08lx\n", uStor));
		uStor->state = TextFree;
		sizeIndex = MAXINDEX;
	}
	assert(sizeIndex >= 0 && sizeIndex <= MAXINDEX);
	uStor->queueIndex = sizeIndex;		/* This is now a smaller block */
	return uStor;
}

/* Routine to return an index into the TwoTable for a given size (round up to next power of two) */
int getSizeIndex(size_t size)
{
	size_t	testSize;
	int	sizeIndex;

	testSize = MAXTWO;
	sizeIndex = MAXINDEX;
	/* Theory here is to hunt for first significant bit. Then if there is more to the word, bump back
	 * to previous queue size. Note that in the following loop, the sizeIndex can go negative if the
	 * value of size is less than MINTWO (which is queue index 0) but since we guarantee there will be a
	 * remainder, we will increment back to 0.
	 */
	while (0 == (testSize & size))
	{
		--sizeIndex;				/* Try next smaller queue */
		if (0 <= sizeIndex)			/* .. if there is a queue */
			testSize >>= 1;
		else					/* Else leave loop with last valid testSize */
			break;
	}
	if (0 != (size & (testSize - 1)))		/* Is there a remainder? */
		++sizeIndex;				/* .. if yes, round up a size */
	return sizeIndex;
}

/* Obtain free storage of the given size */
void *gtm_text_alloc(size_t size)
{
	unsigned char	*retVal;
	textElem 	*uStor, *qHdr;
	size_t		tSize;
	int		sizeIndex, hdrSize;
	boolean_t	reentered;

	/* Note that this if is also structured for maximum fallthru. The else will
	 * be near the end of this entry point.
	 */
	if (gtaSmInitialized)
	{
		hdrSize = OFFSETOF(textElem, userStorage);		/* Size of textElem header */
		GTM64_ONLY(if (MAXUINT4 < (size + hdrSize)) GTMASSERT); /* Only deal with < 4GB requests */
		NON_GTM64_ONLY(if ((size + hdrSize) < size) GTMASSERT); /* Check for wrap with 32 bit platforms */
		assert(hdrSize < MINTWO);

		fast_lock_count++;
		++gtaSmDepth;						/* Nesting depth of memory calls */
		reentered = (1 < gtaSmDepth);
		if (reentered)
		{
			--gtaSmDepth;
			assert(FALSE);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MEMORYRECURSIVE);
		}
		INCR_CNTR(totalAllocs);
		if (0 != size)
		{
			tSize = size + hdrSize;				/* Add in header size */
			if (MAXTWO >= tSize)
			{	/* Use our memory manager for smaller pieces */
				sizeIndex = getSizeIndex(tSize);		/* Get index to size we need */
				assert(0 <= sizeIndex && MAXINDEX >= sizeIndex);
				GET_QUEUED_ELEMENT(sizeIndex, uStor, qHdr);
				tSize = TwoTable[sizeIndex];
				uStor->realLen = (unsigned int)tSize;
			} else
			{	/* Use regular mmap to obtain the piece */
				TEXT_ALLOC(tSize, uStor);
				INCR_SUM(totalRallocGta, tSize);
				uStor->queueIndex = REAL_ALLOC;
				uStor->realLen = (unsigned int)tSize;
				sizeIndex = MAXINDEX + 1;
			}
			INCR_SUM(totalUsedGta, tSize);
			INCR_SUM(totalAllocGta, tSize);
			INCR_CNTR(allocCnt[sizeIndex]);
			uStor->state = TextAllocated;
			retVal = &uStor->userStorage.userStart;
			/* Assert we have an appropriate boundary */
			assert(((long)retVal & (long)IA64_ONLY(-16)NON_IA64_ONLY(-8)) == (long)retVal);
			TRACE_TXTALLOC(retVal, tSize);
		} else	/* size was 0 */
			retVal = &NullStruct.nullStr[0];
		--gtaSmDepth;
		--fast_lock_count;
		return retVal;
	} else  /* Storage mgmt has not been initialized */
	{
		gtaSmInit();
		/* Reinvoke gtm_text_alloc now that we are initialized	*/
		return (void *)gtm_text_alloc(size);
	}
}

/* Release the free storage at the given address */
void gtm_text_free(void *addr)
{
	textElem 	*uStor, *buddyElem;
	int 		sizeIndex, hdrSize, saveIndex;
	size_t		allocSize;

	if (process_exiting)	/* If we are exiting, don't bother with frees. Process destruction can do it */
		return;
	if (!gtaSmInitialized)	/* Storage must be init'd before can free anything */
		GTMASSERT;
	++fast_lock_count;
	++gtaSmDepth;	/* Recursion indicator */
	if (1 < gtaSmDepth)
	{
		--gtaSmDepth;
		assert(FALSE);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MEMORYRECURSIVE);
	}
	INCR_CNTR(totalFrees);
	if ((unsigned char *)addr != &NullStruct.nullStr[0])
	{
		hdrSize = OFFSETOF(textElem, userStorage);
		uStor = (textElem *)((unsigned long)addr - hdrSize);		/* Backup ptr to element header */
		assert(TextAllocated == uStor->state);
		allocSize = uStor->realLen;
		sizeIndex = uStor->queueIndex;
		DECR_SUM(totalUsedGta, uStor->realLen);
		if (sizeIndex >= 0)
		{	/* We can put the storage back on one of our simple queues */
			assert(0 == ((unsigned long)uStor & (TwoTable[sizeIndex] - 1)));	/* Verify alignment */
			assert(0 <= sizeIndex && MAXINDEX >= sizeIndex);
			assert(uStor->realLen == TwoTable[sizeIndex]);
			uStor->state = TextFree;
			INCR_CNTR(freeCnt[sizeIndex]);
			DECR_SUM(totalAllocGta, TwoTable[sizeIndex]);
			/* First, if there are larger queues than this one, see if it has a buddy that it can
			   combine with */
			while (sizeIndex < MAXINDEX)
			{
				buddyElem = (textElem *)((unsigned long)uStor ^ TwoTable[sizeIndex]);/* Address of buddy */
				assert(0 == ((unsigned long)buddyElem & (TwoTable[sizeIndex] - 1)));/* Verify alignment */
				assert(TextAllocated == buddyElem->state || TextFree == buddyElem->state);
				assert(0 <= buddyElem->queueIndex && buddyElem->queueIndex <= sizeIndex);
				if (TextAllocated == buddyElem->state || buddyElem->queueIndex != sizeIndex)
					/* All possible combines done */
					break;

				/* Remove buddy from its queue and make a larger element for a larger queue */
				DEQUEUE_STOR_ELEM(buddyElem);
				if (buddyElem < uStor)		/* Pick lower address buddy for top of new bigger block */
					uStor = buddyElem;
				++sizeIndex;
				assert(0 <= sizeIndex && MAXINDEX >= sizeIndex);
				INCR_CNTR(elemCombines[sizeIndex]);
				uStor->queueIndex = sizeIndex;
			}
			ENQUEUE_STOR_ELEM(sizeIndex, uStor);
		} else
		{
			assert(REAL_ALLOC == sizeIndex);		/* Better be a real alloc type block */
			INCR_CNTR(freeCnt[MAXINDEX + 1]);               /* Count free of malloc */
			TEXT_FREE(uStor, allocSize);
			DECR_SUM(totalRallocGta, allocSize);
			DECR_SUM(totalAllocGta, allocSize);
 		}
		TRACE_TXTFREE(addr, allocSize);
	}
	--gtaSmDepth;
	--fast_lock_count;
}
#endif /* not __MVS__ */

/* Routine to print the end-of-process info -- either allocation statistics or malloc trace dump.
 * Note that the use of FPRINTF here instead of util_out_print is historical. The output was at one
 * time going to stdout and util_out_print goes to stderr. If necessary or desired, these could easily
 * be changed to use util_out_print instead of FPRINTF
 */
void printAllocInfo(void)
{
        textElem        *eHdr, *uStor;
        int             i;

	if (0 == totalAllocs)
		return;		/* Nothing to report -- likely a utility that doesn't use mmap */
	FPRINTF(stderr, "\nMmap small storage performance:\n");
	FPRINTF(stderr,
		"Total allocs: %d, total frees: %d, total ralloc bytes: %ld, max ralloc bytes: %ld\n",
		totalAllocs, totalFrees, totalRallocGta, rAllocMax);
	FPRINTF(stderr,
		"Total (currently) allocated (includes overhead): %ld, Total (currently) used (no overhead): %ld\n",
		totalAllocGta, totalUsedGta);
	FPRINTF(stderr, "\nQueueSize    Allocs     Frees    Splits  Combines    CurCnt    MaxCnt\n");
	FPRINTF(stderr,   "                                                      Free       Free\n");
	FPRINTF(stderr,   "---------------------------------------------------------------------\n");
	{
		for (i = 0; i <= MAXINDEX + 1; ++i)
		{
			FPRINTF(stderr,
				"%9d %9d %9d %9d %9d %9d %9d\n", TwoTable[i], allocCnt[i], freeCnt[i],
				elemSplits[i], elemCombines[i], freeElemCnt[i], freeElemMax[i]);
		}
	}
}
#endif /* COMP_GTA */
