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

#ifndef LIST_FILE_INCLUDED
#define LIST_FILE_INCLUDED

void open_list_file(void);
void close_list_file(void);
void list_chkpage(void);
void list_cmd(void);
void list_head(bool newpage);
void list_line(char *c);
void list_line_number(void);
void list_tab(void);

#endif
