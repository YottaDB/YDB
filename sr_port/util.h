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

#ifndef UTIL_included
#define UTIL_included


boolean_t util_is_log_open(void);

#ifdef VMS
#include <descrip.h>

void util_in_open(struct dsc$descriptor_s *file_prompt);
void util_out_open(struct dsc$descriptor_s *file_prompt);
void util_log_open(char *filename, uint4 len);
#else
void util_in_open(void *);
#endif
void util_cm_print();
void util_exit_handler(void);
void util_out_close(void);
void util_out_print();
void util_out_send_oper(char *addr, unsigned int len);
void util_out_write(unsigned char *addr, unsigned int len);

#endif /* UTIL_included */
