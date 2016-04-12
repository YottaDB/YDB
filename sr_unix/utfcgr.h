/****************************************************************
 *								*
 * Copyright (c) 2015 Fidelity National Information 		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Header file for using UTF8 lookaside cache to aid in scanning of UTF8 strings. Similar
 * in concept to the fnpc.h lookaside cache for $PIECE() performance improvements except
 * here, we are logging mode changes between BADCHARs, UTF8 and ASCII character groups.
 */

#ifndef UTFCGR_INCLUDED
#define UTFCGR_INCLUDED

#include "utfcgr_trc.h"

/* Debugging counters */
#ifdef DEBUG
GBLREF	uint4	process_id;
GBLREF	int	u_miss;				/* UTF cache misses (debug) */
GBLREF	int	u_hit;				/* UTF cache hits (debug) */
GBLREF	int	u_small;			/* UTF scanned small string brute force (debug) */
GBLREF	int	u_pskip;			/* Number of UTF groups "skipped" (debug) */
GBLREF	int	u_puscan;			/* Number of groups "scanned" for located char (debug) */
GBLREF	int	u_pabscan;			/* Number of non-UTF groups we scan for located char (debug) */
GBLREF	int	u_parscan;			/* Number of partial scans (partial cache hits) (debug) */
GBLREF	int	u_parhscan;			/* Number of partial scans after filled slots (debug) */
#  define COUNT_UTF_EVENT(X)	++u_##X;
#  define COUNT_UTF_INCR(X, Y)	u_##X += Y;
#else
#  define COUNT_UTF_EVENT(X)
#  define COUNT_UTF_INCR(X, Y)
#endif

/* Macro to determine the type and length of a character.
 * Arguments:
 *    CPTR    - pointer to first byte of character.
 *    CTOPPTR - pointer to last byte of string (max possible part of char).
 *    CTYPE   - variable where determined type is stored.
 *    CLEN    - length in bytes of character.
 */
#define UTF_CHARTYPELEN(CPTR, CTOPPTR, CTYPE, CLEN)						\
MBSTART {											\
	if (ASCII_MAX >= *(CPTR))								\
	{	/* We have an ASCII char */							\
		CTYPE = UTFCGR_ASCII;								\
		CLEN = 1;									\
	} else											\
		/* We have a UTF8 or BADCHAR type char - bytelen is set appropriately */	\
		CTYPE = ((UTF8_VALID((CPTR), (CTOPPTR), CLEN)) ? UTFCGR_UTF : UTFCGR_BADCHAR);	\
} MBEND

/* Define defaults and limits of the utfcgr structures. Note the defaults here are not chosen by any scientific
 * principles but are our current best guess at what will work for the largest group of customers.
 */
#define GTM_UTFCGR_STRINGS_DEFAULT	 50	/* Default max number of strings to cache scan results for ($gtm_utfcgr_strings) */
#define GTM_UTFCGR_STRINGS_MAX		 254	/* Value is a single byte and 255 is used as "invalid value" flag */
#define GTM_UTFCGR_STRING_GROUPS_DEFAULT 32	/* Default max char groups cached per string ($gtm_utfcgr_string_groups) */
#define UTFCGR_STRLEN_MIN		 33	/* Minimum (byte) length string that creates a cache */
#define UTFCGR_MAX_UTF_LEN (UTFCGR_STRLEN_MIN * 2)	/* Maximum byte length for a UTF8 group in cache - promotes scanning
							 * as have to scan at most this many bytes in a group - allows approx
							 * UTFCGR_STRLEN_MIN UTF8 chars in a string averaging 2 bytes each.
							 */
#define UTFCGR_MAXLOOK_DIVISOR		 5	/* Value to divide into TREF(gtm_utfcgr_strings) to get number of spins to locate
						 * an available cache line (skipping slots with reserve flag set) before we
						 * simply overwrite one. Used to compute TREF(utfcgr_string_lookmax).
						 */
/* Flags for utfcgr.entry[].typflags. If no flags set, group is of unknown type. Note there's no specific purpose for making these
 * actual bit flags - they could just become values but the thought was there might be additional flags in the future where
 * it would matter.
 */
#define UTFCGR_NONE	0x00			/* Group has an as-yet undefined type */
#define UTFCGR_ASCII	0x01			/* Group is all ASCII */
#define UTFCGR_BADCHAR	0x02			/* Group is BADCHAR(s) */
#define UTFCGR_UTF	0x04			/* Group is all UTF8 (no BADCHARs) */
#define UTFCGR_EOL	0x08			/* Used in utfscan_parseblk->char_type to indicate ran into EOL during scan */

/* Structure for each character group in a given string. The typflags field describes the group of characters that start after
 * the end of the last character group until the start of the next character group. All of the characters are of the same type.
 * The charcnt and byteidx fields describe the start (in character count and byte index) of the *next* group of characters.
 * So entry[0] gives the flags for entry[0] but the count/offset for the start of entry[1] when parsing.
 */
typedef struct utfcgr_entry_struct
{
	uint4	typflags:8;			/* Byte full 'o flag bits for THIS group */
	uint4	charcnt:24;			/* Total count of characters at start of next group */
	uint4	byteidx;			/* Offset in bytes from start of entire string to start of next group */
} utfcgr_entry;
/* Structure for each recently used string above the string length minimum that describes the string and
 * notes where its transition points are between BADCHAR, UTF8 and ASCII. We refer to these entries as
 * "groups" but note they are different than $PIECE() type pieces which have a given delimiter. Here,
 * the "groups" are delimited not by characters but by character TYPE.
 */
typedef struct utfcgr_struct
{
	mstr		last_str;		/* The last string (addr/len) we used in cache */
	unsigned short	ngrps;			/* Number of groups for which values are filled in */
	unsigned short	idx;			/* The index of this group in the entry[] array */
	boolean_t	reference;		/* Reference bit(s) to prevent overwrite if possible */
	utfcgr_entry	entry[1]; 		/* Table of  char groups for this string. This is a variable dimension
						 * field - dimension is in TREF(gtm_utfcgr_string_groups).
						 */
} utfcgr;
/* Structure for the entire allocation for UTF scan cache */
typedef struct
{
	utfcgr		*utfcgrsteal;		/* Last stolen cache element */
	utfcgr		*utfcgrmax;		/* (use addrs to avoid array indexing) */
	utfcgr		*utfcgrs;		/* Ptr to variable dimension array which has TREF(gtm_utfcgr_strings) entries */
	uint4		utfcgrsize;		/* Size of 1 utfcgr entry (varies depending on TREF(gtm_utfcgr_string_groups) */
} utfcgr_area;
/* This structure is for the scan descriptor used by the UTF scanning/parsing routines */
typedef struct
{
	mval		*mv;			/* Addr of mval this scan targets. Note this mval should be known to
						 * garbage collection so it is kept up-to-date across GC events (input but
						 * note mv may be updated with mv->utfcgr_indx field set).
						 */
	boolean_t	stoponbadchar;		/* TRUE - stops scan at badchar and returns with next two fields set
						 * FALSE - keeps scanning counting badchars as 1 byte (input)
						 */
	int		scan_byte_offset;	/* Byte offset (0 origin) where scan should start or where it ended (on return).
						 * If 0, starts/ended at beginning and next two fields are ignored. All 3 fields
						 * must be in sync or weird stuff can happen (input and output).
						 */
	int		scan_char_count;	/* Char count (1 origin) of the characters behind scan_byte_offset and does NOT
						 * include the character at that offset.
						 */
	int		utfcgr_indx;		/* utfcgr_entry index (0 origin) where scan should start or where it ended on
						 * return (input and output).
						 */
	int		scan_char_len;		/* Byte length of the character whose offset/index we are returning (input and
						 * output but not updated when scan_char_type is UTFCGR_EOL).
						 */
	int		scan_char_type;		/* Character type of returned character position (output) */
	unsigned char	*badcharstr;		/* Location of badchar if found while scanning (output). Note this field and the
						 * next only updated when scan_char_type is UTFCGR_BADCHAR and we return FALSE.
						 */
	unsigned char	*badchartop;		/* End of string to pass to utf8_badchar() (output) */
} utfscan_parseblk;

/* Entry point declarations */
utfcgr		*utfcgr_getcache(mval *mv);
boolean_t	utfcgr_scanforcharN(int char_num, utfscan_parseblk *utf_parse_blk);
#ifdef DEBUG
void		utfcgr_stats(void);
#endif

#endif
