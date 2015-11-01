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

CONDITION_HANDLER(gtm_maxstr_ch)
{
	START_CH;
	if (maxstr_buff[maxstr_stack_level].addr)
		free(maxstr_buff[maxstr_stack_level].addr);
	maxstr_buff[maxstr_stack_level].addr = NULL;
	maxstr_buff[maxstr_stack_level].len = 0;
	maxstr_stack_level--;
	NEXTCH;
}

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
			maxstr_buff[maxstr_stack_level].addr = (char*)malloc(new_buff_size);
			memcpy(maxstr_buff[maxstr_stack_level].addr, *buff, space_occupied);
			free(*buff);
		} else
		{
			maxstr_buff[maxstr_stack_level].addr = (char*)malloc(new_buff_size);
			memcpy(maxstr_buff[maxstr_stack_level].addr, *buff, space_occupied);
		}
		*buff = maxstr_buff[maxstr_stack_level].addr;
		maxstr_buff[maxstr_stack_level].len = new_buff_size;
	}
	return maxstr_buff[maxstr_stack_level].len;
}
