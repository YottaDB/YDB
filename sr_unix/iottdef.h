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

//kt BEGIN additions -----------------------------------------------------------------------------------

typedef int tty_getsetattr_status;
#define GETSETATTR_SUCCESS  0
#define GETSETATTR_FAILURE -1

//Is a bit ON for a given character in a bitmask array?
// Doesn't address special terminators > MAX_ASCII and/or LS and PS if default_mask_term
#define IS_TERMINATOR(MASK_ARRAY, INPUT_CHAR, utf8_active) 					\
    ({ 												\
        uint4 msk_num = 0; 									\
        uint4 msk_in = 0; 									\
        int result = 0; 									\
        if (!(utf8_active) || (ASCII_MAX) >= (INPUT_CHAR)) { 					\
            msk_num = (uint4)(INPUT_CHAR) / (NUM_BITS_IN_INT4); 				\
            msk_in = (1U << ((uint4)(INPUT_CHAR) % (NUM_BITS_IN_INT4))); 			\
            result = (msk_in & (MASK_ARRAY)[msk_num]); 						\
        } 											\
        result; 										\
    })

#define IS_SPECIAL_TERMINATOR(INPUT_CHAR, utf8_active) 						\
    (utf8_active && ( 										\
        u32_line_term[U32_LT_NL] == INPUT_CHAR || 						\
        u32_line_term[U32_LT_LS] == INPUT_CHAR || 						\
        u32_line_term[U32_LT_PS] == INPUT_CHAR))

//Turn a bit OFF for a given character in a bitmask array
#define TURN_MASK_BIT_OFF(MASK_ARRAY, ACHAR) 							\
    (MASK_ARRAY[(ACHAR) / NUM_BITS_IN_INT4] &= ~(1U << ((ACHAR) % NUM_BITS_IN_INT4)))

//Turn a bit ON for a given character in a bitmask array
#define TURN_MASK_BIT_ON(MASK_ARRAY, ACHAR) 							\
    (MASK_ARRAY[(ACHAR) / NUM_BITS_IN_INT4] |= (1U << ((char) % NUM_BITS_IN_INT4)))

#define SET_BIT_FLAG_BY_BOOL(ABOOL, AFLAG, AVAR)						\
MBSTART { 											\
        if (ABOOL) { 										\
            (AVAR) |= (AFLAG);  /* Set the flag */ 						\
        } else { 										\
            (AVAR) &= ~(AFLAG); /* Clear the flag */ 						\
        } 											\
} MBEND

#define SET_BIT_FLAG_ON(AFLAG, AVAR)							 	\
MBSTART { 											\
            (AVAR) |= (AFLAG);  /* Set the flag */ 						\
} MBEND

#define SET_BIT_FLAG_OFF(AFLAG, AVAR)							 	\
MBSTART { 											\
            (AVAR) &= ~(AFLAG); /* Clear the flag */ 						\
} MBEND

#define BIT_FLAG_IS_ON(AFLAG, AVAR) (((AVAR) & (AFLAG)) != 0)
#define BIT_FLAG_IS_OFF(AFLAG, AVAR) (((AVAR) & (AFLAG)) == 0)

#define STRUCTS_ARE_EQUAL(STRUCT1, STRUCT2)                           				\
    (sizeof(STRUCT1) == sizeof(STRUCT2) &&                                   			\
     memcmp(&(STRUCT1), &(STRUCT2), sizeof(STRUCT1)) == 0 ? 1 : 0)

//Below is variant of macro defined in io.h
#define GET_ADDR_AND_LEN_V2(STRUCT, OFFSET, ADDR, LEN)  					\
     {                                             						\
	 ADDR = (char *)((STRUCT)->str.addr + OFFSET + 1); 					\
	 LEN = (int)(*(STRUCT)->str.addr + OFFSET);        					\
     }

//kt END additions -----------------------------------------------------------------------------------


#define TERMHUP_NOPRINCIO_CHECK(WRITE)								\
MBSTART {											\
	assert(hup_on || prin_in_dev_failure);							\
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
	/*
	In this array [0..7] of 32 bit integers, each bit of each array element is used as a flag for one character.  I.e. 32 bits can represent 32 different characters
	For example, mask[0] is used for chars 0-31 (the control chars), mask[1] is used for chars 32-63, and so on
	To test for a bit in element[0], one can use "if (mask & (1 << INPUT_CHAR)) ...""
	The index in the array can be calculated by: INPUT_CHAR / NUM_BITS_IN_INT4
	The bit mask for the mask[index] element can be calculated by (1 << (INPUT_CHAR % NUM_BITS_IN_INT4))  The modulus(%) accounts for the index'd element.
	Eight (8) elements, of 32 bits each gives total of 256 bits, allowing 1 bit for each of chars 0-255
	*/
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
	uint4	mask;  //Each of 32 bits is a flag for one character (0-31, i.e. the control chars).   E.g. if (mask & (1 << INPUT_CHAR)) ...
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

//kt begin addition ----------
// This struct will hold everything related to a particular TTY IO state of a device.
// See also additional documentation in iott_compile_ttio_struct() in setterm.c
// For TTY IO signals that are never modified by YDB, e.g. ISIG (enable signals), the state will not
//    be stored in separate struct variables.  Instead the combined state will be contained in the contained ttio_struct
typedef struct
{
	//-------------------------------------------------------------------------------------
	// !! NOTICE !! tt_ptr->ttio_struct'            <--- pointer   This was original gt.m code use.
	//		tt_ptr->io_state.ttio_struct    <--- plain struct, not a pointer
	//-------------------------------------------------------------------------------------
	struct termios 		ttio_struct;		/* This will be treated as a COMPILED state, ready to send to TTY IO system.  It is not the master state itself. */
	uint4			ext_cap;
	io_termmask		mask_term;		/* Array of bit flags to mark which characters are input terminators */
	boolean_t		canonical;		/* FYI: Canonical means tty system provides simple line editing (e.g. delete)*/
	boolean_t		devparam_echo;		/* store device parameter echo/noecho state */
	boolean_t		ydb_echo;		/* store if ydb should locally echo/noecho (distinct from TTY IO echo state) */
	boolean_t		hostsync;		/* Store hostsync state */
	boolean_t		ttsync;			/* store ttsync state */
	boolean_t		discard_lf;		/* UTF8 mode - previous char was CR so ignore following LF */
	boolean_t		case_convert;		/* Enables or disables YottaDB from converting lowercase input to uppercase during READs */
	boolean_t		no_type_ahead;		/* store TYPEAHEAD state.  Enables or disables type-ahead buffering for a terminal. */
	boolean_t		passthru;		/* store PASTHRU state.  Enables or disables interpretation of the ERASE character for a terminal. PASTHRU shifts management of handling and response to ERASE characters in the input stream from YottaDB to the application code.*/
	boolean_t		readsync;		/* store READSYNC state.  Enables or disables automatic output of <XON> before a READ and <XOFF> after a READ.*/
	boolean_t		escape_processing;	/* store ESCAPE state.  Enables or disables YottaDB processing of escape sequences.*/
	boolean_t		default_mask_term;	/* mask_term is the default */
} ttio_state;
//kt end addition ----------


typedef struct
{
	uint4			in_buf_sz;		/* size of read buffer		*/
	/* unsigned short  	pg_width;		   width of output page		*/
	uint4			ext_cap;
	io_terminator		enbld_outofbands; 	/* enabled out-of-band chars	*/
	int			fildes;
	ttio_state		io_state;		//kt added -- IO state based on latest USE or OPEN device parameters
	ttio_state		initial_io_state;	//kt added -- IO state from when ydb first started
	ttio_state		direct_mode_io_state;	//kt added -- IO state for use when interacting with user in direct mode
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

//BEGIN ADDITIONS BY //kt --------------------------------------------------------------------------------------------------------------
typedef enum io_params io_params_type;
typedef boolean_t (*devparam_validator)(io_params_type aparam);
typedef void (*set_tty_err_handler)(io_desc* io_ptr, int save_errno, int filedes);

void iott_compile_ttio_struct(io_desc * io_ptr, ttio_state* io_state_ptr, cc_t vtime, cc_t vmin);
tty_getsetattr_status iott_compile_state_and_set_tty_and_ydb_echo(io_desc* ioptr, cc_t vtime, cc_t vmin, set_tty_err_handler an_err_handler);
tty_getsetattr_status iott_set_tty(io_desc* io_ptr, struct termios* ttio_struct_ptr, set_tty_err_handler an_err_handler);
tty_getsetattr_status iott_set_tty_and_ydb_echo(io_desc* io_ptr, struct termios* ttio_struct_ptr, set_tty_err_handler an_err_handler);
void iott_use_params_to_state(io_desc * io_ptr, mval * devparms);
void iott_set_ydb_echo(io_desc* io_ptr, ttio_state* source_io_state_ptr, ttio_state* output_io_state_ptr);
void iott_open_params_to_state(io_desc * io_ptr, mval * devparms, ttio_state* io_state_ptr);
void iott_common_params_to_state(io_desc* io_ptr, ttio_state* io_state_ptr, mval * devparms, devparam_validator is_valid_dev_param);
void iott_TTY_to_state(d_tt_struct * tt_ptr, ttio_state* an_io_state_ptr);
void iott_tio_struct_to_state(ttio_state* an_io_state_ptr, struct termios * ttio_struct_ptr);
void iott_restoreterm(io_desc * io_ptr);
boolean_t iott_is_valid_use_param(io_params_type aparam);
boolean_t iott_is_valid_open_param(io_params_type aparam);
void iott_set_mask_term_conditional(io_desc* io_ptr, io_termmask*  mask_term_ptr, boolean_t bool_test, boolean_t set_default);
ttio_state*  iott_setterm_for_direct_mode(io_desc * io_ptr);
void iott_setterm_for_no_canonical(io_desc * io_ptr,  ttio_state * temp_io_state_ptr);

void handle_set_tty_err_mode_1(io_desc* io_ptr, int save_errno, int filedes);
void handle_set_tty_err_mode_2(io_desc* io_ptr, int save_errno, int filedes);
void handle_set_tty_err_mode_3(io_desc* io_ptr, int save_errno, int filedes);
void handle_reset_tty_err(io_desc* io_ptr, int save_errno, int filedes);

//END ADDITIONS BY //kt --------------------------------------------------------------------------------------------------------------

#endif
