/****************************************************************
 *								*
 *	Copyright 2010, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __FILE_INPUT_INPUT_H__
#define __FILE_INPUT_INPUT_H__

#define	FILE_INPUT_GET_ERROR		-1
#define	FILE_INPUT_GET_LINE2LONG	-2
#define DO_RTS_ERROR_TRUE 		TRUE
#define DO_RTS_ERROR_FALSE 		FALSE

typedef enum {
    IOP_EOL	= 0x00,
    IOP_REWIND 	= 0x01
} open_params_flags;
void file_input_init(char *fn, short fn_len, open_params_flags params_flag);
void file_input_close(void);
void file_input_bin_init(char *line1_ptr, int line1_len);
int file_input_bin_get(char **in_ptr, off_t *file_offset, char **buff_base, boolean_t do_rts_error);
int file_input_bin_read(void);
int file_input_get_xchar(char *in_ptr, int max_chars_to_read);
int file_input_read_xchar(char *in_ptr, int max_chars_to_read);
int file_input_get(char **in_ptr, int max_len);

#endif
