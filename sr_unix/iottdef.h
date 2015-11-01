/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef IOTTDEF_H
#define IOTTDEF_H

#include "gtm_termios.h"
#include "gtm_stdio.h"

#define TERM_MSK	0x08002400	/* CR LF ESC */
#define TERM_MSK_UTF8_0	0x08003400	/* add FF */
#define TERM_MSK_UTF8_4	0x00000020	/* NL */

#include "iottdefsp.h"

#define NUM_BITS_IN_INT4	(sizeof(int4) * 8)

#define TTDEF_BUF_SZ 		1024
#define TTDEF_PG_WIDTH 		255

#define IOTT_FLUSH_WAIT		300
#define IOTT_FLUSH_RETRY	50
#define IOTT_BUFF_LEN		3072
#define IOTT_BUFF_MIN		128

#define TT_EDITING		0x1000
#define TT_NOINSERT		0x2000

typedef struct
{
	uint4	mask[8];
} io_termmask;

typedef struct
{
	unsigned short	status;
	unsigned short	char_ct;
	uint4		dev_dep_info;
}iosb;

typedef struct
{
	uint4	x;
	uint4	mask;
}io_terminator;

typedef struct
{
	uint4		in_buf_sz;		/* size of read buffer		*/
	/* unsigned short  pg_width;		   width of output page		*/
	uint4		ext_cap;
	io_terminator	enbld_outofbands; 	/* enabled out-of-band chars	*/
	uint4   	term_ctrl;
	io_termmask	mask_term;
	int		fildes;
	struct termios  *ttio_struct;
	char		*ttybuff;		/* buffer for tty */
	volatile char	*tbuffp;		/* next open space in buffer */
	volatile boolean_t	timer_set;	/* text flush timer is set */
	volatile boolean_t	write_active;	/* we are in write -- postpone flush by timer */
	boolean_t	canonical;
	mstr		recall_buff;		/* if EDITING enabled */
	int		recall_size;		/* size of recall_buff allocated */
	int		recall_width;		/* display width of current contents */
	boolean_t	discard_lf;		/* UTF8 mode - previous char was CR so ignore following LF */
	boolean_t	default_mask_term;	/* mask_term is the default */
	boolean_t	done_1st_read;		/* UTF8 mode - check for BOM if not */
}d_tt_struct;

void iott_flush_buffer(io_desc *ioptr, boolean_t new_write_flag);
void iott_mterm(io_desc *ioptr);
void iott_rterm(io_desc *ioptr);
void iott_readfl_badchar(mval *vmvalptr, wint_t *dataptr32, int datalen,
			 int delimlen, unsigned char *delimptr, unsigned char *strend);
#endif
