/****************************************************************
 *								*
 * Copyright (c) 2001-2026 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef STPG_SORT_INCLUDED
#define STPG_SORT_INCLUDED

void stpg_sort(mstr **base, mstr **top);
void stpg_sort_new_contig(mstr_sort_array_element *base, mstr_sort_array_element *top);
void stpg_sort_new(mstr_sort_array *array_p, size_t base, size_t top);

#endif /* STPG_SORT_INCLUDED */
