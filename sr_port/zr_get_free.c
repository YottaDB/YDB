/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "zbreak.h"

char *zr_get_free(z_records *z_ptr, char *addr)
{
	z_records temp;
	char *c_ptr;
	char *dst, *src;

	/* NOTE: records are stored by decreasing addresses */
	assert(z_ptr->free < z_ptr->end);
	for (c_ptr = z_ptr->beg;  c_ptr <= z_ptr->free;  c_ptr += z_ptr->rec_size)
	{
		if (*(int4 *)c_ptr == (int)addr)
			return (c_ptr);
		else  if (*(int4 *)c_ptr < (int)addr || c_ptr == z_ptr->free)	/* insert here */
		{
			z_ptr->free += z_ptr->rec_size;	/* space for 1 record */
			if (z_ptr->free == z_ptr->end)	/* expand if necessary */
			{
				temp = *z_ptr;
				zr_init(z_ptr, 2 * (z_ptr->end - z_ptr->beg) / z_ptr->rec_size, z_ptr->rec_size);
				assert(2 * (temp.end - temp.beg) == (z_ptr->end - z_ptr->beg));
				memcpy(z_ptr->beg, temp.beg, temp.end - temp.beg);
				assert(z_ptr->free == z_ptr->beg);
				z_ptr->free += temp.free - temp.beg;
				c_ptr = c_ptr - temp.beg + z_ptr->beg;
				free(temp.beg);
			}
			/* shift records down into the bottom spot which was allocated*/
			for (dst = z_ptr->free - z_ptr->rec_size, src = dst - z_ptr->rec_size;  dst > c_ptr;
				dst -= z_ptr->rec_size, src -= z_ptr->rec_size)
					memcpy(dst, src, z_ptr->rec_size);
			memset(c_ptr, 0, z_ptr->rec_size);
			*(int4 *)c_ptr = (int)addr;
			assert(dst == c_ptr);
			return (c_ptr);
		}
	}
	GTMASSERT;
}
