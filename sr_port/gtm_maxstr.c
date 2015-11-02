/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_string.h"
#include "error.h"
#include "gtm_maxstr.h"

/* GT.M string buffer stack where each entry is allocated geometrically on the need basis */
GBLDEF	mstr	maxstr_buff[MAXSTR_STACK_SIZE];
GBLDEF  int	maxstr_stack_level = -1;

/* This handler is to release the heap storage allocated within a function in case of errors.
 * The suggested protocol is that after a function is done with using the string buffer, the macro
 * MAXSTR_BUFF_FINI used in the function epilog releases the malloc'd storage. But if an error
 * occurs within the function body, the function epilog is not executed so this condition handler
 * should do the same so that the storage is not kept dangling. */
CONDITION_HANDLER(gtm_maxstr_ch)
{
	START_CH;
	if (maxstr_buff[maxstr_stack_level].addr)
	{
		free(maxstr_buff[maxstr_stack_level].addr);
		maxstr_buff[maxstr_stack_level].addr = NULL;
	}
	maxstr_buff[maxstr_stack_level].len = 0;
	maxstr_stack_level--;
	NEXTCH;
}

/* The routine checks if the currently available buffer (either 32K automatic or >32K malloc'd)
 * is sufficient for the new space requirement. If not sufficient, it keeps doubling the buffer size
 * and checks until a new buffer size is large enough to accommodate the incoming string. It then
 * allocates the new buffer and releases the previously malloc'd buffer.
 * Note that if strings fit within 32K, the automating buffer is used and no heap storage is used.
 * Parameters:
 *
 * 	space_needed - buffer space needed to accommodate the string that is about to be written
 * 	buff - address of the pointer to the beginning of the buffer. If reallocation occurs, buff will be
 * 		modified to point to the reallocated buffer.
 * 	space_occupied - how full is the buffer?
 *
 * Returns the new allocated buffer size.
 */
int gtm_maxstr_alloc(int space_needed, char** buff, int space_occupied)
{
	int	new_buff_size;

	if (space_needed > (maxstr_buff[maxstr_stack_level].len - space_occupied))
	{ /* Existing buffer is not sufficient. reallocate the buffer with the double the size */
		new_buff_size = maxstr_buff[maxstr_stack_level].len;
		while (space_needed > new_buff_size - space_occupied)
			new_buff_size += new_buff_size;
		if (NULL != maxstr_buff[maxstr_stack_level].addr)
		{
			assert(maxstr_buff[maxstr_stack_level].addr == *buff);
			maxstr_buff[maxstr_stack_level].addr = (char *)malloc(new_buff_size);
			memcpy(maxstr_buff[maxstr_stack_level].addr, *buff, space_occupied);
			free(*buff);
		} else
		{
			maxstr_buff[maxstr_stack_level].addr = (char *)malloc(new_buff_size);
			memcpy(maxstr_buff[maxstr_stack_level].addr, *buff, space_occupied);
		}
		*buff = maxstr_buff[maxstr_stack_level].addr;
		maxstr_buff[maxstr_stack_level].len = new_buff_size;
	}
	return maxstr_buff[maxstr_stack_level].len;
}
