/****************************************************************
 *								*
 *	Copyright 2005 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef IOTT_EDIT_H
#define IOTT_EDIT_H

int	write_str(unsigned char *str, unsigned short len, unsigned short cur_x, bool move);
int	move_cursor_left (unsigned short col);
int	move_cursor_right (unsigned short col);
int	write_loop(int fildes, unsigned char *str, int num_times);
int	move_cursor(int fildes, int num_up, int num_left);

#endif
