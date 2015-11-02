/****************************************************************
 *								*
 *	Copyright 2007, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GTM_TEXT_ALLOC_H
#define GTM_TEXT_ALLOC_H

/* The below macros are used to allocate and release areas of storage that will be used
   to contain executable code. Traditionally, GTM has just used malloc() and free() and
   placed this code on the heap but some systems now protect against that (noteably
   Linux). On those platforms (and any others we choose to separate the executable storage
   pools from the regular heap storage), we use gtm_text_alloc() and gtm_text_free() to
   allocate and free executable storage. These modules use mmap() and munmap() for this
   purpose.

   Note that while use of the mmap() interface for executable storage does have some
   potential advantages for security (and as noted above, is required for some platforms
   to even function, because of the page aligned granularity of its requests, storage usage
   with gtm_text_alloc() is not as efficient as it is with regular heap based storage. For
   this reason, we only use this method on the required platforms rather than all. Replaceing
   the algorithms in gtm_text_alloc.c with ones not based on the buddy system could
   potentially alleviate these efficiency differences.
*/
#if defined(__linux__) || defined(__osf__)
#  define GTM_TEXT_ALLOC(x) gtm_text_alloc(x)
#  define GTM_TEXT_FREE(x) gtm_text_free(x)
void *gtm_text_alloc(size_t size);
void gtm_text_free(void *addr);
void printAllocInfo(void);
#  define COMP_GTA	/* Build gtm_text_alloc() module */
# else
#  define GTM_TEXT_ALLOC(x) gtm_malloc(x)
#  define GTM_TEXT_FREE(x) gtm_free(x)
#endif

#endif /* GTM_TEXT_ALLOC_H */
