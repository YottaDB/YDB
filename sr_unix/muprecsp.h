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

#ifndef __MUPRECSP_H__
#define __MUPRECSP_H__

int4 mur_close(mur_rab *r);
int4 mur_fopen(mur_rab *r, char *fna, int fnl);
int4 mur_fread_eof(mur_rab *r, char *fna, int fnl);
int4 mur_get_first(mur_rab *r);
int4 mur_get_last(mur_rab *r);
void mur_output_status(int4 status);
void mur_open_files_error(ctl_list *curr, int fd);


#endif
