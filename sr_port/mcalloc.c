/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "mmemory.h"
#include "min_max.h"

GBLREF int 		mcavail;
GBLREF mcalloc_hdr 	*mcavailptr, *mcavailbase;

char *mcalloc(unsigned int n)
{
	mcalloc_hdr	*hdr, *nxt, *ptr;
	int		new_size, rel_size;

	/* Choice of char_ptr_t made because it is a 64 bit pointer on Tru64 which
	 * is the alignment we need there, or any other 64 bit platforms we support
	 * in the future.
	 */
 	n = ROUND_UP2(n, SIZEOF(char_ptr_t));

	if (n > mcavail)
	{ 	/* No sufficient space in the current block. Follow the link and check if the next block has sufficient
		 * space.  There is no next block or the next one doesn't have enough space, allocate a new block with
		 * the requested size and insert it after the current block.
		 */
		hdr = mcavailptr->link;
		if (NULL == hdr || n > hdr->size)
		{
			if (NULL != hdr)
			{ 	/* i.e. the next block doesn't have sufficient space for n. Release as many small blocks as
				 * necessary to make up for the space that we are allocating for the large block.
				 * By release several small blocks and replacing them with a large block ensures that total
				 * memory footprint is not increased due to a rare occurence of large routine compilation.
				 */
				rel_size = 0;
				for (nxt = hdr; NULL != nxt && (rel_size += nxt->size) < n; nxt = ptr)
				{
					ptr = nxt->link;
					free(nxt);
				}
			} else
				nxt = NULL;
			new_size = (int)MAX(MC_DSBLKSIZE, (n + MCALLOC_HDR_SZ));
			hdr = (mcalloc_hdr *)malloc(new_size);
			hdr->link = nxt;
			hdr->size = (int4)(new_size - MCALLOC_HDR_SZ);
			mcavailptr->link = hdr;
		}
		assert(n <= hdr->size);
		memset(&hdr->data[0], 0, hdr->size);
		mcavailptr = hdr;
		mcavail = hdr->size;
	}
	mcavail -= n;
	assert(mcavail >= 0);
	return &mcavailptr->data[mcavail];
}
