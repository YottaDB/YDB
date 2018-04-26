/****************************************************************
 *								*
 * Copyright 2001, 2014 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdlib.h"

#include <curses.h>		/* must be before term.h */

#include "gtm_term.h"
#include "getcaps.h"
#include "gtm_sizeof.h"
#if defined(__MVS__) && __CHARSET_LIB==1	/* -qascii */
#include "ebc_xlat.h"
#endif
#include "ydb_getenv.h"

#undef	KEY_UP
#undef	KEY_DOWN
#undef	KEY_RIGHT
#undef	KEY_LEFT
#undef	KEY_BACKSPACE
#undef	KEY_DC

#define	MAXCOLS	132
#define	BEL	'\007'

GBLDEF	int	AUTO_RIGHT_MARGIN;	/* auto margins */
GBLDEF	char	*CLR_EOS;		/* clear to end of display */
GBLDEF	char	*CLR_EOL;		/* clear to end of line */
GBLDEF	int	COLUMNS;		/* number of columns */
GBLDEF	char	*CURSOR_ADDRESS;	/* cursor motion */
GBLDEF	char	*CURSOR_DOWN;		/* cursor down */
GBLDEF	char	*CURSOR_LEFT;		/* cursor left */
GBLDEF	char	*CURSOR_RIGHT;		/* cursor right */
GBLDEF  char	*CURSOR_UP;		/* cursor up */
GBLDEF	int	EAT_NEWLINE_GLITCH;	/* newline glitch */
GBLDEF	char	*KEY_BACKSPACE;		/* backspace key */
GBLDEF	char	*KEY_DC;		/* delete key */
GBLDEF	char	*KEY_DOWN;		/* down arrow key */
GBLDEF	char	*KEY_LEFT;		/* left arrow key */
GBLDEF	char	*KEY_RIGHT;		/* right arrow key */
GBLDEF	char	*KEY_UP;		/* up arrow key */
GBLDEF	char	*KEY_INSERT;		/* insert key aka KEY_IC */
GBLDEF	char	*KEYPAD_LOCAL;		/* turn keypad off */
GBLDEF	char	*KEYPAD_XMIT;		/* turn keypad on */
GBLDEF  int	GTM_LINES;		/* number of rows */

#ifdef KEEP_zOS_EBCDIC
#pragma convlit(suspend)
#endif
static	int	gtm_auto_right_margin = 0;
static	char	gtm_clr_eos[] = "\033[J";
static	char	gtm_clr_eol[] = "\033[K";
static	int	gtm_columns = 80;
static	char	gtm_cursor_address[] = "\033[%i%p1%d;%p2%dH";
static	char	gtm_cursor_down[] = "\012";	/* <Ctrl-J> */
static	char	gtm_cursor_left[] = "\010";
static	char	gtm_cursor_right[] = "\033[C";
static	char	gtm_cursor_up[] = "\033[A";
static	int	gtm_eat_newline_glitch = 1;
static	char	gtm_key_backspace[] = "\010";
static	char	gtm_key_dc[] = "\033[3~";
static	char	gtm_key_down[] = "\033OB";
static	char	gtm_key_left[] = "\033OD";
static	char	gtm_key_right[] = "\033OC";
static	char	gtm_key_up[] = "\033OA";
static	char	gtm_key_insert[] = "";
static	char	gtm_keypad_local[] = "\033[?1l";
static	char	gtm_keypad_xmit[] = "\033[?1h";
static	int	gtm_lines = 24;

#if defined(__MVS__) && __CHARSET_LIB==1	/* -qascii */
static	char	gtm_cap_ascii[16 * 16];	/* ESC_LEN from io.h times number of tigetstr values */
#define CAP2ASCII(CAP)							\
{									\
	ebc_len = strlen(CAP) + 1;					\
	assert(SIZEOF(gtm_cap_ascii) > (gtm_cap_index + ebc_len));	\
	ebc_to_asc((unsigned char *)&gtm_cap_ascii[gtm_cap_index], (unsigned char *)CAP, ebc_len);	\
	CAP = &gtm_cap_ascii[gtm_cap_index];				\
	gtm_cap_index += ebc_len;					\
}
#else
#define CAP2ASCII(CAP)
#endif

#ifdef KEEP_zOS_EBCDIC
#pragma convlit(resume)
#endif
/* extern	unsigned char	leftkey[], rightkey[], upkey[], downkey[]; */

/* --------------------------------------------------------------
 * returns the status whether successful or not.
 * status =	 1	if successful
 *		 0	if TERM is not present in terminfo
 *		-1	if terminfo database could not be opened
 * --------------------------------------------------------------
 */

int	getcaps(int fildes)
{
	char		*cap;
#if defined(__MVS__) && __CHARSET_LIB==1	/* -qascii */
	char		cap_ebcdic[128];	/* more than enough for terminal name */
	int		ebc_len, gtm_cap_index = 0;
#endif
	int		status;

	cap = ydb_getenv(YDBENVINDX_GENERIC_TERM, NULL_SUFFIX, NULL_IS_YDB_ENV_MATCH);
	if (!cap)
		cap = "unknown";

#if defined(__MVS__) && __CHARSET_LIB==1	/* -qascii */
	ebc_len = strlen(cap);
	if (SIZEOF(cap_ebcdic) < ebc_len)
		ebc_len = SIZEOF(cap_ebcdic) - 1;
	asc_to_ebc((unsigned char *)cap_ebcdic, (unsigned char *)cap, ebc_len);
	cap_ebcdic[ebc_len] = '\0';
	cap = cap_ebcdic;
#endif

	SETUPTERM(cap, fildes, &status);

	if (1 == status)
	{

#ifdef KEEP_zOS_EBCDIC
#pragma convlit(suspend)
#endif
#if defined(__MVS__) && __CHARSET_LIB==1	/* -qascii */
/* assumes source is EBCDIC and curses/terminfo entries expect EBCDIC */
#pragma convert(source)
#endif
		AUTO_RIGHT_MARGIN = tigetflag("am");
		CLR_EOS = tigetstr("ed");
		CLR_EOL = tigetstr("el");
		COLUMNS = tigetnum("cols");
		CURSOR_ADDRESS = tigetstr("cup");
		CURSOR_DOWN = tigetstr("cud1");
		CURSOR_LEFT = tigetstr("cub1");
		CURSOR_RIGHT = tigetstr("cuf1");
		CURSOR_UP = tigetstr("cuu1");
		EAT_NEWLINE_GLITCH = tigetflag("xenl");
		KEY_BACKSPACE = tigetstr("kbs");
		KEY_DC = tigetstr("kdch1");
		KEY_DOWN = tigetstr("kcud1");
		KEY_LEFT = tigetstr("kcub1");
		KEY_RIGHT = tigetstr("kcuf1");
		KEY_UP = tigetstr("kcuu1");
		KEY_INSERT = tigetstr("kich1");
		KEYPAD_LOCAL = tigetstr("rmkx");
		KEYPAD_XMIT = tigetstr("smkx");
		GTM_LINES = tigetnum("lines");

#if defined(__MVS__) && __CHARSET_LIB==1	/* -qascii */
#pragma convert(pop)
#endif
#ifdef KEEP_zOS_EBCDIC
#pragma convlit(resume)
#endif
		assert(-1 != AUTO_RIGHT_MARGIN);
		assert((char *)-1 != CLR_EOS);
		CAP2ASCII(CLR_EOS);
		assert((char *)-1 != CLR_EOL);
		CAP2ASCII(CLR_EOL);
		assert(-2 != COLUMNS);
		assert((char *)-1 != CURSOR_ADDRESS);
		CAP2ASCII(CURSOR_ADDRESS);
		assert((char *)-1 != CURSOR_DOWN);
		CAP2ASCII(CURSOR_DOWN);
		assert((char *)-1 != CURSOR_LEFT);
		CAP2ASCII(CURSOR_LEFT);
		assert((char *)-1 != CURSOR_RIGHT);
		CAP2ASCII(CURSOR_RIGHT);
		assert((char *)-1 != CURSOR_UP);
		CAP2ASCII(CURSOR_UP);
		assert((char *)-1 != KEY_BACKSPACE);
		CAP2ASCII(KEY_BACKSPACE);
		assert((char *)-1 != KEY_DC);
		CAP2ASCII(KEY_DC);
		assert((char *)-1 != KEY_DOWN);
		CAP2ASCII(KEY_DOWN);
		assert((char *)-1 != KEY_LEFT);
		CAP2ASCII(KEY_LEFT);
		assert((char *)-1 != KEY_RIGHT);
		CAP2ASCII(KEY_RIGHT);
		assert((char *)-1 != KEY_UP);
		CAP2ASCII(KEY_UP);
		assert((char *)-1 != KEY_INSERT);
		CAP2ASCII(KEY_INSERT);
		assert((char *)-1 != KEYPAD_LOCAL);
		CAP2ASCII(KEYPAD_LOCAL);
		assert((char *)-1 != KEYPAD_XMIT);
		CAP2ASCII(KEYPAD_XMIT);
		assert(-2 != GTM_LINES);
	}
	else
	{
		AUTO_RIGHT_MARGIN = gtm_auto_right_margin;
		CLR_EOS = gtm_clr_eos;
		CLR_EOL = gtm_clr_eol;
		COLUMNS = gtm_columns;
		CURSOR_ADDRESS = gtm_cursor_address;
		CURSOR_DOWN = gtm_cursor_down;
		CURSOR_LEFT = gtm_cursor_left;
		CURSOR_RIGHT = gtm_cursor_right;
		CURSOR_UP = gtm_cursor_up;
		EAT_NEWLINE_GLITCH = gtm_eat_newline_glitch;
		KEY_BACKSPACE = gtm_key_backspace;
		KEY_DC = gtm_key_dc;
		KEY_DOWN = gtm_key_down;
		KEY_LEFT = gtm_key_left;
		KEY_RIGHT = gtm_key_right;
		KEY_UP = gtm_key_up;
		KEY_INSERT = gtm_key_insert;
		KEYPAD_LOCAL = gtm_keypad_local;
		KEYPAD_XMIT = gtm_keypad_xmit;
		GTM_LINES = gtm_lines;
	}
	return status;
}
