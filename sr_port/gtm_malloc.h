/****************************************************************
 *								*
 *	Copyright 2003, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GTM_MALLOC_H__included
#define GTM_MALLOC_H__included

#define GTM_MEMORY_RESERVE_DEFAULT 64

/* While this memory manager is capable of allocating greater than 4GB chunks of storage (should one have the
   requisite resources for that), the current model stores the allocated size in the storage header for maintaining
   allocation byte counts and such. Unless this header is expanded, it cannot accomodate greater than a 4GB field
   (32 bits unsigned). Hence this gtm_malloc() is restricted to dealing with a max 4GB maximum single allocation.
*/
typedef unsigned int gtm_msize_t;

/* Each allocated block has the following structure. The actual address
   returned to the user for 'malloc' and supplied by the user for 'free'
   is actually the storage beginning at the 'userStorage.userStart' area.
   This holds true even for storage that is truely malloc'd. Note that true
   allocated length is kept even in the pro header.
*/
typedef struct storElemStruct
{	/* While the following chars and short are not the best for performance, they enable us
	   to keep the header size to 8 bytes in a pro build. This is important since our minimum
	   allocation size is 16 bytes leaving 8 bytes for data. Also I have not researched what
	   they are, there are a bunch of 8 byte allocates in GT.M that if we were to go to a 16
	   byte header would make the minimum block size 32 bytes thus doubling the storage
	   requirements for these small blocks. SE 03/2002 [Note 16 byte header is the norm in 64 bit]
	*/
	signed char	queueIndex;			/* Index into TwoTable for this size of element */
	unsigned char	state;				/* State of this block */
	unsigned short	extHdrOffset;			/* For MAXTWO sized elements: offset to the
							   header that describes the extent */
	gtm_msize_t	realLen;			/* Real (total) length of allocation */
#ifdef DEBUG
	struct	storElemStruct	*fPtr;			/* Next storage element on free/allocated queue */
	struct	storElemStruct	*bPtr;			/* Previous storage element on free/allocated queue */
	unsigned char	*allocatedBy;			/* Who allocated storage */
	NON_GTM64_ONLY(unsigned int smTn;)		/* Storage management transaction number allocated at */
        GTM64_ONLY(gtm_uint64_t smTn;)			/* Avoid padding in GTM64 by using large int */
	gtm_msize_t	allocLen;			/* Requested length of allocation */
	unsigned char	headMarker[4];			/* Header that should not be modified during usage */
	union
	{
		struct storElemStruct *deferFreeNext;	/* Pointer to next deferred free block */
		unsigned char	userStart;		/* First byte of user useable storage */
	} userStorage;
#else
	GTM64_ONLY(char filler[8];)                     /* For 64 bit systems, the user area needs to be 16 byte aligned */
	union						/* In production mode, the links are used only when element is free */
	{
		struct storElemStruct *deferFreeNext;	/* Pointer to next deferred free block */
		struct					/* Free block information */
		{
			struct	storElemStruct	*fPtr;	/* Next storage element on free queue */
			struct	storElemStruct	*bPtr;	/* Previous storage element on free queue */
		} links;
		unsigned char	userStart;		/* First byte of user useable storage */
	} userStorage;
#endif
} storElem;

void verifyFreeStorage(void);
void verifyAllocatedStorage(void);
void raise_gtmmemory_error(void);

#define VERIFY_STORAGE_CHAINS			\
{						\
	GBLREF uint4	gtmDebugLevel;		\
	if (GDL_SmFreeVerf & gtmDebugLevel)	\
		verifyFreeStorage();		\
	if (GDL_SmAllocVerf & gtmDebugLevel)	\
		verifyAllocatedStorage();	\
}

#endif /* GTM_MALLOC_H__included */
