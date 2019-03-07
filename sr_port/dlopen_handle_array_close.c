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

#include <dlfcn.h>

#include "dlopen_handle_array.h"

GBLREF	void_ptr_t	*dlopen_handle_array;
GBLREF	uint4		dlopen_handle_array_len_alloc;
GBLREF	uint4		dlopen_handle_array_len_used;

void	dlopen_handle_array_close(void)
{
	void_ptr_t	handle;
	int		i, status;

	assert(dlopen_handle_array_len_used <= dlopen_handle_array_len_alloc);
	for (i = 0; i < dlopen_handle_array_len_used; i++)
	{
		handle = dlopen_handle_array[i];
		assert(NULL != handle);
		status = dlclose(handle);
		assert(0 == status);
		/* Not much we can do in case of an error in "dlclose" while we are already in "ydb_exit". Silently ignore. */
	}
	/* Reset globals so they can be used afresh in case another "ydb_init" happens in the same process. */
	if (NULL != dlopen_handle_array)
	{
		free(dlopen_handle_array);
		dlopen_handle_array = NULL;
	}
	dlopen_handle_array_len_alloc = 0;
	dlopen_handle_array_len_used = 0;
}
