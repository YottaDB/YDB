/****************************************************************
 *								*
 *	Copyright 2010, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __FILE_INPUT_INPUT_H__
#define __FILE_INPUT_INPUT_H__

void file_input_init(char *fn, short fn_len);
void file_input_close(void);
int file_input_bin_get(char **in_ptr, ssize_t *file_offset, char **buff_base);
int file_input_bin_read(void);
int file_input_get(char **in_ptr);

#endif
