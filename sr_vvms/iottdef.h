/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __IOTTDEF_H__
#define __IOTTDEF_H__

#define IO_FUNC_R (IO$_READVBLK | IO$M_EXTEND)
#define IO_FUNC_W IO$_WRITEVBLK
#define TERM_MSK 0x00002000	/* <RET> */

#define SHFT_MSK 0x00000001
#define CTRL_B 2
#define CTRL_U 21
#define CTRL_Z 26

#include "iottdefsp.h"

#define ESC 27

#define TTMAX_PG_LENGTH	255
#define TTMAX_PG_WIDTH	511
#define TTDEF_PG_WIDTH 	255
#define TTDEF_BUF_SZ 	1024

#define TTEOL "\015\012"
#define TERMINAL_STATIC_ASTS 2
#define IOSB_BUF_SZ	20
#define RING_BUF_SZ	2048
#define MIN_RINGBUF_SP	128
#define MIN_IOSB_SP	3
#define SPACE_INUSE	768
#define MAX_MEMCPY	512

#define io_space(x,y) (x >= y ? RING_BUF_SZ - (x - y) : (y - x))

#define sb_dist(x,y) (x >= y ? IOSB_BUF_SZ - (x - y) : (y - x))

#define new_sbfree(a) ((a)->sb_buffer + (((a)->sb_free - (a)->sb_buffer) % IOSB_BUF_SZ))

/* ***************************************************** */
/* *********** structure for terminal driver *********** */
/* ***************************************************** */

typedef struct
{	unsigned short status;
	unsigned short char_ct;
	unsigned char term_char;
	unsigned char reserved;
	unsigned char term_length;
	unsigned char cur_pos_eol;
}read_iosb;


typedef struct
{	unsigned short	status;
	unsigned short	char_ct;
	uint4	dev_dep_info;
}iosb;


typedef struct
{
	read_iosb iosb_val;
	char *start_addr;
}iosb_struct;

typedef struct
{	uint4	mask[8];
} io_termmask;

typedef struct
{	uint4	x;
	uint4	mask;
}io_terminator;

typedef struct
{
	unsigned short buf_len;
	unsigned short item_code;
	char *addr;
	char *ret_addr;
}item_list_struct;

typedef struct
{

	short 		channel;	/* channel for access to terminal */
	unsigned char	clock_on;	/* flag for clock running or off  */
	uint4	read_mask;	/* arg mask used in sys$qio read  */
	uint4	write_mask;	/* arg mask for sys$qio write	  */
	uint4	in_buf_sz;	/* size of read buffer		  */
	/* unsigned short  pg_width;	 width of output page		  */
	uint4	term_char;
	uint4	ext_cap;
	uint4	term_tab_entry;	/* SMG index for terminal type    */
	io_terminator	enbld_outofbands; /*enabled out-of-band chars*/
	unsigned char	read_timer;
	item_list_struct item_list[6];
	uint4   item_len;
	unsigned char	*io_inuse;	/* output buffer pointer to area  */
					/* in use but not qio'd		  */
	unsigned char   *io_free;	/* pointer to free space in buff  */
	unsigned char	*io_pending;	/* pointer to data already queued */
	unsigned char 	*io_buftop;
	unsigned char	*io_buffer;	/* the write ring buffer	  */
	/* pending must follow free - must be able to get pair uninterrupted with movq */
	iosb_struct	*sb_free;	/* first free position in buffer  */
	iosb_struct	*sb_pending;	/* pointer to first iosb that 	  */
					/* return data after qio is done  */

	iosb_struct	*sb_buftop;
	iosb_struct	*sb_buffer;	/* the iosb ring buffer		  */
	mstr		erase_to_end_line;
	mstr		key_up_arrow;
	mstr		key_down_arrow;
	mstr		clearscreen;
	read_iosb	stat_blk;
	bool		term_chars_twisted;
	uint4	ctrlu_msk;
}d_tt_struct;

typedef struct
{	unsigned char	class;
	unsigned char	type;
	unsigned short	pg_width;
	unsigned int	term_char : 24;
	unsigned int	pg_length : 8;
	uint4	ext_cap;	/* extended terminal characteristics */
}t_cap;	/* terminal capabilites */

void iott_cancel_read(io_desc *io_ptr);
void iott_clockfini(d_tt_struct *tt_ptr);
void iott_resetast(io_desc *io_ptr);
void iott_wtclose(d_tt_struct *tt_ptr);
void iott_wtctrlu(short v, io_desc *iod);
void iott_wtstart(d_tt_struct *tt_ptr);
void iott_wtfini(d_tt_struct *tt_ptr);

#endif
