/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "compiler.h"
#include "iott_setterm.h"

#define TERM_MSK	0x08002400	/* CR LF ESC */
#define TERM_MSK_UTF8_0	0x08003400	/* add FF */
#define TERM_MSK_UTF8_4	0x00000020	/* NL */

#include "iottdefsp.h"

#define NUM_BITS_IN_INT4	(SIZEOF(int4) * 8)

#define TTDEF_BUF_SZ 		MAX_SRCLINE
#define TTDEF_PG_WIDTH 		255

#define IOTT_FLUSH_WAIT		(300 * (uint8)NANOSECS_IN_MSEC)
#define IOTT_FLUSH_RETRY	(50 * (uint8)NANOSECS_IN_MSEC)
#define IOTT_BUFF_LEN		3072
#define IOTT_BUFF_MIN		128

#define TT_EDITING		0x1000
#define TT_NOINSERT		0x2000
#define TT_EMPTERM		0x4000

#define TERMHUP_NOPRINCIO_CHECK(WRITE)								\
MBSTART {											\
	assert(hup_on || prin_in_dev_failure);										\
	exi_condition = -ERR_TERMHANGUP;							\
	ISSUE_NOPRINCIO_IF_NEEDED(io_ptr, WRITE, FALSE);					\
	async_action(FALSE);									\
} MBEND

GBLREF	uint4	process_id;

#define	IS_SETTERM_DONE(IOPTR)	((tt == IOPTR->type) && (dev_open == IOPTR->state)				\
					&& (NULL != IOPTR->dev_sp)						\
					&& (process_id == ((d_tt_struct *)IOPTR->dev_sp)->setterm_done_by))

/* If we are going to read from the terminal, and "iott_setterm" has not yet been done, do it first.
 * This is needed so we get nocanonical and noecho mode turned on. That way we see the keystrokes as the user types them in,
 * with no translation (e.g. escape sequences etc.) done by the terminal driver.
 */
#define	SETTERM_IF_NEEDED(ioPtr, ttPtr)							\
MBSTART {										\
	/* Only the true runtime runs with the modified terminal settings */		\
	assert(ttPtr == ((d_tt_struct *)ioPtr->dev_sp));				\
	if (0 == ttPtr->setterm_done_by)						\
		iott_setterm(ioPtr);							\
} MBEND

#define	EXPECT_SETTERM_DONE_FALSE	FALSE
#define	EXPECT_SETTERM_DONE_TRUE	TRUE

/* Below is converse of SETTERM_IF_NEEDED */
#define	RESETTERM_IF_NEEDED(ioPtr, expectSettermDone)				\
MBSTART {									\
	assert(!expectSettermDone || IS_SETTERM_DONE(ioPtr));			\
	if (IS_SETTERM_DONE(ioPtr))						\
		iott_resetterm(ioPtr);						\
	assert(!IS_SETTERM_DONE(ioPtr));					\
} MBEND

#define	TT_UNFLUSHED_DATA_LEN(TT_PTR)	(TT_PTR->tbuffp - TT_PTR->ttybuff)

enum	tt_which
{
	ttwhichinvalid,
	dmread,
	ttread,
	ttrdone
};

typedef struct
{
	enum tt_which	who_saved;
	unsigned char	*buffer_start;		/* initial stringpool.free */
	wint_t		*buffer_32_start;
	int		utf8_more;
	int		dx;
	int		dx_start;
	int		dx_instr;
	int		dx_outlen;
	int		instr;
	int		outlen;
	int		index;				/* dm_read only */
	int		cl;				/* dm_read only */
	int		length;
	int		exp_length;
	int		recall_index;			/* index corresponding to input string that was recalled when interrupted */
	int		no_up_or_down_cursor_yet;
	int		utf8_seen;
	boolean_t	insert_mode;
	ABS_TIME	end_time;
	unsigned char	*zb_ptr;
	unsigned char	*zb_top;
	unsigned short	escape_length;			/* dm_read only */
	unsigned char	escape_sequence[ESC_LEN];	/* dm_read only */
	unsigned char	more_buf[GTM_MB_LEN_MAX + 1];
#	ifdef DEBUG
	boolean_t	timed;
	uint8		nsec_timeout;
#	endif
} tt_interrupt;

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
	int	nchars;		/* In M mode this is # of "unsigned char" sized bytes in the string pointed to by "buff".
				 * In UTF-8 mode, this is the # of "wint_t" sized codepoints in the string pointed to by "buff".
				 */
	int	width;		/* Total display width of the "nchars" characters */
	int	bytelen;	/* Total byte length of the "nchars" characters */
	char	*buff;		/* In M mode, this points to the array of bytes comprising the input string.
				 * In UTF-8 mode, this points to an array of "wint_t" sized codepoints comprising the input string.
				 */
} recall_ctxt_t;

typedef struct
{
	uint4			in_buf_sz;		/* size of read buffer		*/
	/* unsigned short  	pg_width;		   width of output page		*/
	uint4			ext_cap;
	io_terminator		enbld_outofbands; 	/* enabled out-of-band chars	*/
	uint4			term_ctrl;
	io_termmask		mask_term;
	int			fildes;
	struct termios		*ttio_struct;
	tt_interrupt		tt_state_save;		/* in case job interrupt */
	boolean_t		mupintr;		/* read was interrupted */
	char			*ttybuff;		/* buffer for tty */
	volatile char		*tbuffp;		/* next open space in buffer */
	recall_ctxt_t		*recall_array;		/* if EDITING enabled, this points to MAX_RECALL-sized array of
							 * previously input strings.
							 */
	int			recall_index;		/* Offset into circular "recall_array" pointing one past
							 * to most recent input.
							 */
	volatile boolean_t	timer_set;		/* text flush timer is set */
	volatile boolean_t	write_active;		/* we are in write -- postpone flush by timer */
	boolean_t		canonical;
	boolean_t		discard_lf;		/* UTF8 mode - previous char was CR so ignore following LF */
	boolean_t		default_mask_term;	/* mask_term is the default */
	boolean_t		done_1st_read;		/* UTF8 mode - check for BOM if not */
	pid_t			setterm_done_by;	/* if non-zero, points to pid that did "iott_setterm";
							 * used to later invoke "iott_resetterm" if needed.
							 */
}d_tt_struct;

void iott_flush_buffer(io_desc *ioptr, boolean_t new_write_flag);
void iott_mterm(io_desc *ioptr);
void iott_rterm(io_desc *ioptr);
void iott_readfl_badchar(mval *vmvalptr, wint_t *dataptr32, int datalen,
			 int delimlen, unsigned char *delimptr, unsigned char *strend, unsigned char *buffer_start);
void	iott_recall_array_add(d_tt_struct *tt_ptr, int nchars, int width, int bytelen, void *ptr);
#endif
