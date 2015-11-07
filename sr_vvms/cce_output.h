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

#ifndef __CCE_OUTPUT_H__
#define  __CCE_OUTPUT_H__

void cce_out_open(void);
void cce_out_write(unsigned char *addr, unsigned int len);
void cce_out_close(void);
void cce_read_return_channel(void);

#endif
