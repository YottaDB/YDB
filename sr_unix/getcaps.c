/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* ------------------------------------------------------
 * mdef.h not included because the definition of bool
 * conflicts with that in curses.h in the AIX platform
 * also mdef.h is not required here except for the GBLDEF and assert
 * ------------------------------------------------------
 */

#include "gtm_stdlib.h"
#include <curses.h>		/* must be before term.h */
#include "gtm_term.h"
#include "getcaps.h"
#include "gtm_sizeof.h"
#ifdef DEBUG
#include <assert.h>
#endif
#if defined(__MVS__) && __CHARSET_LIB==1	/* -qascii */
#include "ebc_xlat.h"
#endif

#ifndef assert
#define assert(x)
#endif

#define GBLDEF

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
static	char	gtm_clr_eos[] = "[J";
static	char	gtm_clr_eol[] = "[K";
static	int	gtm_columns = 80;
static	char	gtm_cursor_address[] = "[%i%p1%d;%p2%dH";
static	char	gtm_cursor_down[] = "\015";	/* <Ctrl-M> */
static	char	gtm_cursor_left[] = "";
static	char	gtm_cursor_right[] = "OC";
static	char	gtm_cursor_up[] = "OA";
static	int	gtm_eat_newline_glitch = 1;
static	char	gtm_key_backspace[] = "";
static	char	gtm_key_dc[] = "";
static	char	gtm_key_down[] = "OB";
static	char	gtm_key_left[] = "OD";
static	char	gtm_key_right[] = "OC";
static	char	gtm_key_up[] = "OA";
static	char	gtm_key_insert[] = "";
static	char	gtm_keypad_local[] = "[?1l";
static	char	gtm_keypad_xmit[] = "[?1h";
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
	char	*cap;
#if defined(__MVS__) && __CHARSET_LIB==1	/* -qascii */
	char	cap_ebcdic[128];	/* more than enough for terminal name */
	int	ebc_len, gtm_cap_index = 0;
#endif
	int	status;

	cap = GETENV("TERM");
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
		if (0 == AUTO_RIGHT_MARGIN)
			AUTO_RIGHT_MARGIN = gtm_auto_right_margin;
		assert((char *)-1 != CLR_EOS);
		if (NULL == CLR_EOS || (char *)-1 == CLR_EOS)
			CLR_EOS = gtm_clr_eos;
		else
			CAP2ASCII(CLR_EOS);
		assert((char *)-1 != CLR_EOL);
		if (NULL == CLR_EOL || (char *)-1 == CLR_EOL)
			CLR_EOL = gtm_clr_eol;
		else
			CAP2ASCII(CLR_EOL);
		assert(-2 != COLUMNS);
		if (-1 == COLUMNS)
			COLUMNS = gtm_columns;
		assert((char *)-1 != CURSOR_ADDRESS);
		if (NULL == CURSOR_ADDRESS || (char *)-1 == CURSOR_ADDRESS)
			CURSOR_ADDRESS = gtm_cursor_address;
		else
			CAP2ASCII(CURSOR_ADDRESS);
		assert((char *)-1 != CURSOR_DOWN);
		if (NULL == CURSOR_DOWN || (char *)-1 == CURSOR_DOWN)
			CURSOR_DOWN = gtm_cursor_down;
		else
			CAP2ASCII(CURSOR_DOWN);
		assert((char *)-1 != CURSOR_LEFT);
		if (NULL == CURSOR_LEFT || (char *)-1 == CURSOR_LEFT)
			CURSOR_LEFT = gtm_cursor_left;
		else
			CAP2ASCII(CURSOR_LEFT);
		assert((char *)-1 != CURSOR_RIGHT);
		if (NULL == CURSOR_RIGHT || (char *)-1 == CURSOR_RIGHT)
			CURSOR_RIGHT = gtm_cursor_right;
		else
			CAP2ASCII(CURSOR_RIGHT);
		assert((char *)-1 != CURSOR_UP);
		if (NULL == CURSOR_UP || (char *)-1 == CURSOR_UP)
			CURSOR_UP = gtm_cursor_up;
		else
			CAP2ASCII(CURSOR_UP);
		assert((char *)-1 != KEY_BACKSPACE);
		if (NULL == KEY_BACKSPACE || (char *)-1 == KEY_BACKSPACE)
			KEY_BACKSPACE = gtm_key_backspace;
		else
			CAP2ASCII(KEY_BACKSPACE);
		assert((char *)-1 != KEY_DC);
		if (NULL == KEY_DC || (char *)-1 == KEY_DC)
			KEY_DC = gtm_key_dc;
		else
			CAP2ASCII(KEY_DC);
		assert((char *)-1 != KEY_DOWN);
		if (NULL == KEY_DOWN || (char *)-1 == KEY_DOWN)
			KEY_DOWN = gtm_key_down;
		else
			CAP2ASCII(KEY_DOWN);
		assert((char *)-1 != KEY_LEFT);
		if (NULL == KEY_LEFT || (char *)-1 == KEY_LEFT)
			KEY_LEFT = gtm_key_left;
		else
			CAP2ASCII(KEY_LEFT);
		assert((char *)-1 != KEY_RIGHT);
		if (NULL == KEY_RIGHT || (char *)-1 == KEY_RIGHT)
			KEY_RIGHT = gtm_key_right;
		else
			CAP2ASCII(KEY_RIGHT);
		assert((char *)-1 != KEY_UP);
		if (NULL == KEY_UP || (char *)-1 == KEY_UP)
			KEY_UP = gtm_key_up;
		else
			CAP2ASCII(KEY_UP);
		assert((char *)-1 != KEY_INSERT);
		if (NULL == KEY_INSERT || (char *)-1 == KEY_INSERT)
			KEY_INSERT = gtm_key_insert;
		else
			CAP2ASCII(KEY_INSERT);
		assert((char *)-1 != KEYPAD_LOCAL);
		if (NULL == KEYPAD_LOCAL || (char *)-1 == KEYPAD_LOCAL)
			KEYPAD_LOCAL = gtm_keypad_local;
		else
			CAP2ASCII(KEYPAD_LOCAL);
		assert((char *)-1 != KEYPAD_XMIT);
		if (NULL == KEYPAD_XMIT || (char *)-1 == KEYPAD_XMIT)
			KEYPAD_XMIT = gtm_keypad_xmit;
		else
			CAP2ASCII(KEYPAD_XMIT);
		assert(-2 != GTM_LINES);
		if (-1 == GTM_LINES)
			GTM_LINES = gtm_lines;
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
