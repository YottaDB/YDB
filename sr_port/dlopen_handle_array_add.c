/****************************************************************
 *								*
 * Copyright (c) 2018 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "dlopen_handle_array.h"

GBLREF	void_ptr_t	*dlopen_handle_array;
GBLREF	uint4		dlopen_handle_array_len_alloc;
GBLREF	uint4		dlopen_handle_array_len_used;

void	dlopen_handle_array_add(void_ptr_t handle)
{
	void_ptr_t	*tmpArray;

	assert(NULL != handle);
	assert(dlopen_handle_array_len_used <= dlopen_handle_array_len_alloc);
	if (dlopen_handle_array_len_alloc == dlopen_handle_array_len_used)
	{	/* No space to hold input handle. Expand array. */
		if (!dlopen_handle_array_len_alloc)
		{	/* Nothing allocated till now. Start at 4. Is not too many and yet should avoid expansion in most cases */
			dlopen_handle_array_len_alloc = 4;
		} else
			dlopen_handle_array_len_alloc *= 2;	/* Array already exists. Allocate twice that size. */
		tmpArray = malloc(SIZEOF(void_ptr_t) * dlopen_handle_array_len_alloc);
		if (NULL != dlopen_handle_array)
		{
			assert(dlopen_handle_array_len_used);
			memcpy(tmpArray, dlopen_handle_array, SIZEOF(void_ptr_t) * dlopen_handle_array_len_used);
			free(dlopen_handle_array);
		}
		dlopen_handle_array = tmpArray;
	}
	dlopen_handle_array[dlopen_handle_array_len_used++] = handle;
}
