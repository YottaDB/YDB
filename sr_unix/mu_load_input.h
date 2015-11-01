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

#ifndef __MU_LOAD_INPUT_H__
#define __MU_LOAD_INPUT_H__

void mu_load_init(char *fn, short fn_len);
void mu_load_close(void);
short	mu_bin_get(char **in_ptr);
int mu_bin_read(void);
short	mu_load_get(char **in_ptr);

#endif
