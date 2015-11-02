/****************************************************************
 *								*
 *	Copyright 2003, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GTM_MALLOC_H__included
#define GTM_MALLOC_H__included

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
	   requirements for these small blocks. SE 03/2002
	*/
	signed char	queueIndex;			/* Index into TwoTable for this size of element */
	unsigned char	state;				/* State of this block */
	unsigned short	extHdrOffset;			/* For MAXTWO sized elements: offset to the
							   header that describes the extent */
	int		realLen;			/* Real (total) length of allocation */
#ifdef DEBUG
	struct	storElemStruct	*fPtr;			/* Next storage element on free/allocated queue */
	struct	storElemStruct	*bPtr;			/* Previous storage element on free/allocated queue */
	unsigned char	*allocatedBy;			/* Who allocated storage */
	int		allocLen;			/* Requested length of allocation */
	unsigned int	smTn;				/* Storage management transaction number allocated at */
	unsigned char	headMarker[4];			/* Header that should not be modified during usage */
	union
	{
		struct storElemStruct *deferFreeNext;	/* Pointer to next deferred free block */
		unsigned char	userStart;		/* First byte of user useable storage */
	} userStorage;
#else
#ifdef GTM64
	char             filler[8];                      /* For 64 bit systems, the user area needs to be 16 byte aligned */
#endif /* GTM64 */
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

/* Malloc of areas containing executable code need to be handled differently on some platforms
   (currently only Linux but likely others in the future)
*/
#if defined(__linux__) && defined(__ia64)
#  define GTM_TEXT_MALLOC(x) gtm_text_malloc(x)
void *gtm_text_malloc(size_t size);
# else
#  define GTM_TEXT_MALLOC(x) gtm_malloc(x)
#endif

void verifyFreeStorage(void);
void verifyAllocatedStorage(void);

#define VERIFY_STORAGE_CHAINS			\
{						\
	GBLREF uint4	gtmDebugLevel;		\
	if (GDL_SmFreeVerf & gtmDebugLevel)	\
		verifyFreeStorage();		\
	if (GDL_SmAllocVerf & gtmDebugLevel)	\
		verifyAllocatedStorage();	\
}

#endif /* GTM_MALLOC_H__included */
