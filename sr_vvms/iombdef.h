/****************************************************************
 *								*
 *	Copyright 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef __IOMBDEF_H
#define __IOMBDEF_H

#define DEF_MB_MAXMSG	255
#define DEF_MB_LENGTH	66

/* *************************************************************** */
/* ***********	structure for the mailbox driver  ***************  */
/* *************************************************************** */

typedef struct
{	unsigned short status;
	unsigned short char_ct;
	uint4 pid;
} mb_iosb;

typedef struct
{
	unsigned short	channel;
	uint4		maxmsg;
	uint4		promsk;
	unsigned char	del_on_close;
	bool		timer_on;
	unsigned char	prmflg;
	uint4		read_mask;
	uint4		write_mask;
	unsigned char	*inbuf;
	unsigned char	*in_pos;
	unsigned char	*in_top;
	mb_iosb		stat_blk;
} d_mb_struct;

void iomb_cancel_read(d_mb_struct *mb_ptr);

#endif
