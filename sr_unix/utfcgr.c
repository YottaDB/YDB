/****************************************************************
 *								*
 * Copyright (c) 2015-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtmimagename.h"

#include "utfcgr.h"
#include "gtm_utf8.h"

GBLREF boolean_t	gtm_utf8_mode;

/* Macro to activate to see contents of cache at start and/or end of each utfcgr_scanforcharN() call. Just
 * uncomment the desired macros below. This trace macro resident here as it should only be used in utfcgr.c.
 */
/* #define DUMP_UTFCACHE_AT_START */
/* #define DUMP_UTFCACHE_AT_END */ /* This one is typically more "useful" */
#define DUMP_UTFCACHE(MVALPTR, UTFCGRP)											\
MBSTART {	/* Note assumption made that this is valid cache for this string so don't use before that's true */	\
	utfcgr_entry	*UTFCGREP;											\
	int		i;												\
															\
	DBGFPF((stderr, "**utfcgr_scanforcharN cache dump for mval 0x"lvaddr" with string address 0x"lvaddr		\
		" char len: %d  byte len:%d\n", (MVALPTR), (MVALPTR)->str.addr, (MVALPTR)->str.char_len,		\
		(MVALPTR)->str.len));											\
	DBGFPF((stderr, "**utfcgr_scanforcharN ngrps: %d, idx: %d, reference: %d - at line %d\n", (UTFCGRP)->ngrps,	\
		(UTFCGRP)->idx, (UTFCGRP)->reference, __LINE__));							\
	for (UTFCGREP = &(UTFCGRP)->entry[0], i = 1; i <= (UTFCGRP)->ngrps; UTFCGREP++, i++)				\
	{														\
		DBGFPF((stderr, "**utfcgr_scanforcharN elem[%d] - typflags: %d, charcnt: %d, byteidx: %d\n",		\
			i, UTFCGREP->typflags, UTFCGREP->charcnt, UTFCGREP->byteidx));					\
	}														\
} MBEND
#ifdef DUMP_UTFCACHE_AT_START
# define DUMP_UTFCACHE_START(MVALPTR, UTFCGRPTR) DUMP_UTFCACHE(MVALPTR, UTFCGRPTR)
#else
# define DUMP_UTFCACHE_START(MVALPTR, UTFCGRPTR)
#endif
#ifdef DUMP_UTFCACHE_AT_END
# define DUMP_UTFCACHE_END(MVALPTR, UTFCGRPTR) DUMP_UTFCACHE(MVALPTR, UTFCGRPTR)
#else
# define DUMP_UTFCACHE_END(MVALPTR, UTFCGRPTR)
#endif

#ifdef UNICODE_SUPPORTED
/* Routine to locate a reusable cache area in the utfcgr array.
 *
 * Parameter:
 *
 *   - mv - mval to which the allocated slot is assigned.
 *
 * Return value:
 *
 *   - Address of utfcgr struct to use (mv is set to use this struct).
 */
utfcgr *utfcgr_getcache(mval *mv)
{
	utfcgr		*utfcgrp;
	int		tries;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	for (utfcgrp = (TREF(utfcgra)).utfcgrsteal, tries = TREF(utfcgr_string_lookmax); 0 < tries;
	     utfcgrp += (TREF(utfcgra)).utfcgrsize, tries--)
	{	/* Look for available slots within the number of slots we are allowed to look at */
		if ((TREF(utfcgra)).utfcgrmax < utfcgrp)
			utfcgrp = (TREF(utfcgra)).utfcgrs;	/* Wrap around to beginning */
		if (utfcgrp->reference)
		{
			utfcgrp->reference = FALSE;
			continue;				/* Ignore this slot for now */
		} else
			break;					/* Found one not reserved */
	}
	if (0 == tries)
	{	/* We stopped due to hitting our search limit so utfcgrp could be past our array */
		if ((TREF(utfcgra)).utfcgrmax < utfcgrp)
			utfcgrp = (TREF(utfcgra)).utfcgrs;	/* Wrap around to beginning */
	}
	/* Doesn't matter if utfcgrsteal is off end - gets fixed when it is picked up for the next getcache() */
	(TREF(utfcgra)).utfcgrsteal = (utfcgr *)((UINTPTR_T)utfcgrp + (TREF(utfcgra)).utfcgrsize);
	mv->utfcgr_indx = utfcgrp->idx + 1;			/* Point supplied mval to new slot - always stored + 1 */
	utfcgrp->last_str = mv->str;				/* Save mstr info */
	utfcgrp->ngrps = 1;					/* Initialize group in use */
	utfcgrp->reference = FALSE;
	return utfcgrp;
}

/* Routine to scan string in UTF8 mode finding a given character number using (and/or creating) lookaside
 * entries in the relevant string cache.
 *
 * Parameters:
 *
 *   char_num      - index of character to be found.
 *   utf_parse_blk - parse block describing string and where scan starts/left-off. Note see utfcgr.h to see which fields in the
 *                   utf_parse_blk are used for input, return, or both.
 *
 * Returns:
 *   - Return code is TRUE is character was found, FALSE if it wasn't.
 *   - utf_parse_blk->scan_char_type describes what was found or not found:
 *	 a) Contains UTFCGR_EOL if ran off end of string before char_num char was found.
 *       b) Contains UTFCGR_BADCHAR if ran into a BADCHAR and utf_parse_blk->stoponbadchar was set which also sets
 *          utf_parse_blk->badcharstr and utf_parse_blk->badchartop to describe the buffer containing the badchar. If
 *          utf_parse_blk->stoponbadchar is not set, this value can be returned with a successful return which also then sets
 *          the "regular" return fields.
 *       c) Contains the character type of the located character which also sets the "regular" return vars.
 *
 * Note - this routine recognizes BADCHARs only in the portion of the string it "skips" to get to the
 * required character position. So for example, if a string is 2 "good" characters followed by a badchar,
 * locating positions 1, 2, or 3 won't trigger a badchar error but locating a char in position 4 would.
 *
 * The utfcgr_entry array is a series of groups of like characters. The value of typflags in a given group is the type of
 * the characters in that group but the other fields define the END of the group. For example, take the following string:
 *
 *     x="123"_$char(195,196)_"4"
 *
 * This would create 3 group entries as follows:
 *
 * utfcgr_entry[0]:
 *   typeflags = UTFCGR_ASCII
 *   charcnt = 3
 *   byteidx = 3
 * utfcgr_entry[1]:
 *   typeflags = UTFCGR_UTF
 *   charcnt = 5
 *   byteidx = 7
 * utfcgr_entry[2]:
 *   typeflags = UTFCGR_ASCII
 *   charcnt = 6
 *   byteidx = 8
 *
 * When the utfcgr.entry array for a given string is full, scanning past what is defined in the cache is dealt with by
 * good 'ole brute force scanning. The u_parhscan counter tracks these but only in debug mode (for now).
 *
 * Notes:
 *
 *   1. This routine does not obey badchar_inhibit but rather behaves according to the utf_parse_blk->stoponbadchar flag in
 *      the utf_parse_blk to have a bit of versatility. For example, if used eventually in an IO routine, we would pass TRUE
 *  	in utf_parse_blk->stoponbadchar regardless of the setting of badchar_inhibit.
 *
 *   2. When a cache is setup for a given string, it survives only until that cache entry is reused.
 *
 *   3. Stringpool garbage collection erases the cache because that is the only way to make sure the address/length
 *      cache validation check is assured to always be correct without false positives.
 *
 *   4. Routine has counters and indexes. Typically, when we are talking about byte indexes, we are talking
 *      about a 0 origin index since that's how the C language references them. When talking about characters
 *      we are generally talking about counts and since characters are not directly addressable because they
 *      are multi-byte in UTF8 mode, these generally have a 1 origin.
 */
boolean_t utfcgr_scanforcharN(int char_num, utfscan_parseblk *utf_parse_blk)
{
	int		char_idx, tcharcnt, tbyteidx, gcharcnt, gbytecnt, bytecnt, skip, bidx;
	int		lchar_byteidx, lchar_charcnt;
	unsigned int	utfcgrepcnt, utfcgridx, chartype, lchar_typflags;
	boolean_t	noslots, lastcharbad, cachemod, scaneol;
	unsigned char	*scantop, *scanptr;
	mval		*mv;
	utfcgr		*utfcgrp;
	utfcgr_entry	*utfcgrep, *utfcgrep_prev, *utfcgrep0, *pcentmax, *pcuentmax;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(gtm_utf8_mode);
	/* Determine mval and its utfcgr index */
	mv = utf_parse_blk->mv;						/* Mval address related to this parse block */
	DBGUTFC((stderr, "  utfcgr_scanforcharN: Entry - char_num: %d - utf_parse_blk->(scan_byte_offset: %d, "
		 "scan_char_count: %d, utfcgr_indx: %d)\n", char_num, utf_parse_blk->scan_byte_offset,
		 utf_parse_blk->scan_char_count, utf_parse_blk->utfcgr_indx));
	DBGUTFC((stderr, "                       .. for string at 0x"lvaddr" char_len: %d  byte len: %d\n",
		 utf_parse_blk->mv->str.addr, utf_parse_blk->mv->str.char_len, utf_parse_blk->mv->str.len));
	assert(0 < char_num);
	assert(NULL != mv);						/* Make sure an mval was specified */
	char_idx = char_num - 1;
	scantop = (unsigned char *)mv->str.addr + mv->str.len;
	utf_parse_blk->scan_char_type = UTFCGR_NONE;			/* Init return char type */
	/* In the below parsing, the following values are involved in the parse and cache:
	 *
	 *   char_num  - Entry parameter specifying which character to locate.
	 *   pcentmax  - Pointer to last available entry[] in this cache line.
	 *   pcuentmax - Pointer to last used entry[] + 1 in this cache line.
	 *   scanptr   - Points to current scan byte in string.
	 *   scantop   - Points to end of current scan string + 1.
	 *   tbyteidx  - The byte index from start of string to current byte or start of current group depending on use.
	 *   tcharcnt  - The character count from the start of the string to current character (but not including it) or
	 *               the start of the current group depending on use.
	 *   utfcgrep  - Points to entry[] in a given utfcgrp string-cache line.
	 *   utfcgrp   - Points to a given string-cache line (utfcgrs[]).
	 *
	 * Note - If we are looking for the "first" char in the string, just fast path down to the non-cache layer so we
	 *        don't confuse the cacheing logic by initializing a null group. Also avoid cacheing during compilation
	 *	  to avoid confusing things.
	 */
	if (IS_MCODE_RUNNING && (UTFCGR_STRLEN_MIN < mv->str.len) && (0 < char_idx))
	{	/* This string meets minimum length requirements for having a cache attached to it */
		utfcgridx = mv->utfcgr_indx - 1;			/* Always stored in utfcgr_indx with 1 origin - adjust */
		/* Point to cache entry for this string */
		utfcgrp = (utfcgr *)((char *)(TREF(utfcgra)).utfcgrs + ((TREF(utfcgra)).utfcgrsize * utfcgridx));
		tcharcnt = tbyteidx = 0;				/* Init counters */
		/* Validate cache entry for this string still intact (index valid, string addr/len the same) */
		if ((TREF(gtm_utfcgr_strings) > utfcgridx) && (utfcgrp->last_str.addr == mv->str.addr)
		    && (utfcgrp->last_str.len == mv->str.len))
		{	/* Cache validated */
			DUMP_UTFCACHE_START(mv, utfcgrp);
			COUNT_UTF_EVENT(hit);
			utfcgrp->reference = TRUE;			/* Mark element recently used */
			pcentmax = &utfcgrp->entry[TREF(gtm_utfcgr_string_groups)]; /* Pointer to last entry + 1 in this row */
			pcuentmax = &utfcgrp->entry[utfcgrp->ngrps];	/* Pointer to last used entry + 1 in this row */
			assert(pcentmax >= pcuentmax);
			utfcgrep0 = &utfcgrp->entry[0];
			/* To pickup from a previous scan here we need both a good offset and a non-negative index */
			if ((0 < utf_parse_blk->scan_byte_offset) && (0 <= utf_parse_blk->utfcgr_indx)
			    && (UTFCGR_NONE != utf_parse_blk->scan_char_type))
			{	/* Picking up from a previous scan - Set up scanning vars appropriately */
				assert(mv->str.len > utf_parse_blk->scan_byte_offset);
				assert(utfcgrp->ngrps > utf_parse_blk->utfcgr_indx);
				utfcgrep = &utfcgrp->entry[utf_parse_blk->utfcgr_indx];
				DEBUG_ONLY(utfcgrepcnt = utf_parse_blk->utfcgr_indx);
				if (utfcgrep != utfcgrep0)
				{	/* This is not the first group so we can reference previous group */
					tcharcnt = (utfcgrep - 1)->charcnt;
					tbyteidx = (utfcgrep - 1)->byteidx;
				} /* else, these values just stay at their initialized (0'd) state set earlier */

			} else
			{
				utfcgrep = utfcgrep0;
				DEBUG_ONLY(utfcgrepcnt = 0);
			}
			/* See if char we want is in cache. Note - even if the char is in the cache, we still need to search for
			 * it within the final character group.
			 *
			 * Note currently the loop below serially processes the entries but it might be possible to speed things up
			 * further by implementing a binary search to find the group we want since the values associated with each
			 * group and the ending values of this group and the start of the next. This would only be useful for long
			 * strings with lots of mixing or lots of UTF8.
			 */
			for (; (utfcgrep < pcuentmax)  && (char_num > utfcgrep->charcnt);
			     tcharcnt = utfcgrep->charcnt, tbyteidx = utfcgrep->byteidx, utfcgrep++)
			{
				COUNT_UTF_EVENT(pskip);			/* Counting "skipped" groups */
				assert(0 != utfcgrep->typflags);	/* Should be *some* indication of content */
				assert(TREF(gtm_utfcgr_string_groups) >= ++utfcgrepcnt); /* Check we're not overflowing array */
				if (utf_parse_blk->stoponbadchar && (UTFCGR_BADCHAR & utfcgrep->typflags))
				{	/* Found BADCHARs in the string and this scan can't tolerate them */
					DBGUTFC((stderr, "  utfcgr_scanforcharN: Returning due to BADCHAR\n"));
					utf_parse_blk->badcharstr = (unsigned char *)utf_parse_blk->mv->str.addr + tbyteidx;
					utf_parse_blk->badchartop = scantop;
					utf_parse_blk->scan_char_type = UTFCGR_BADCHAR;
					DUMP_UTFCACHE_END(mv, utfcgrp);
					return FALSE;
				}
			}
			/* At this point we have one of two cases:
			 *
			 *   1. The current block contains the character we want.
			 *   2. The loop went past the last used entry.
			 *
			 * In case 1, we need a short/fast brute-force search of this character block for the character. We
			 * do that in-line here. Note "search" is simply an index if this is an ASCII or BADBLOCK type
			 * group of characters.
			 *
			 * In case 2, we set things up to fall into the continued search to locate the character
			 * filling in additional cache entries if they are available.
			 *
			 * First up - Case 1, which we hope is the more common case (reduced scanning).
			 */
			if (utfcgrep < pcuentmax)
			{	/* Case 1:  We didn't exit loop due to overflow so char we want is in this block.
				 * Locate the character and return its info.
				 */
				DBGUTFC((stderr, "  utfcgr_scanforcharN: Char located in cache\n"));
				assert(0 != utfcgrep->typflags);
				if (UTFCGR_UTF != utfcgrep->typflags)
				{	/* Group is single byte chars - index to char to return */
					COUNT_UTF_EVENT(pabscan);	/* Count groups we scan for located char */
					assert(char_num >= tcharcnt);
					bidx = char_idx - tcharcnt;	/* Simple offset into this group of char we seek */
					utf_parse_blk->scan_byte_offset = tbyteidx + bidx;
					utf_parse_blk->scan_char_count = tcharcnt + bidx;
					utf_parse_blk->scan_char_len = 1;
					utf_parse_blk->scan_char_type = utfcgrep->typflags;
				} else
				{	/* Group is multi-byte chars - search for char to return */
					COUNT_UTF_EVENT(puscan);	/* Count groups we're scanning for located char */
					scanptr = (unsigned char *)mv->str.addr + tbyteidx;
					for (skip = char_idx - tcharcnt, gcharcnt = 0; ((0 < skip) && (scanptr < scantop));
					     skip--, scanptr += bytecnt, gcharcnt++)
					{	/* Advance the string to locate the desired character. Since we are scanning
						 * an alleged UTF8 string, no BADCHARs should exist in it - hence assertpro().
						 */
						assertpro(UTF8_VALID(scanptr, scantop, bytecnt));
					}
					/* Find length of char we end up on */
					assertpro(UTF8_VALID(scanptr, scantop, bytecnt));
					utf_parse_blk->scan_byte_offset = scanptr - (unsigned char *)mv->str.addr;
					utf_parse_blk->scan_char_count = tcharcnt + gcharcnt;
					utf_parse_blk->scan_char_len = bytecnt;
					utf_parse_blk->scan_char_type = UTFCGR_UTF;
				}
				utf_parse_blk->utfcgr_indx = utfcgrep - utfcgrep0; /* Saved as 0-origin since picked up same way */
				DUMP_UTFCACHE_END(mv, utfcgrp);
				return TRUE;
			}
			/* Case 2 - We ran off the edge of the string or the cache. There are two subcases here:
			 *
			 *   2a. We ran out of text - nothing left to scan - character position doesn't exist in this string.
			 *   2b. The cache doesn't describe the entire string so there is more to scan and potentially add
			 *       to the cache if the necessary entries exist.
			 *
			 * In case 2a, the character was not found so return UTFCGR_EOL as the reason code.
			 *
			 * In case 2b, set up our vars and fall into the same path as the below "start from scratch" scan
			 * to continue building the cache.
			 */
			if (tbyteidx >= mv->str.len)
			{	/* Case 2a - character position is not found (ran out of text) - update output fields */
				DBGUTFC((stderr, "  utfcgr_scanforcharN: Ran off end of scanned string - EOL\n"));
				utf_parse_blk->scan_byte_offset = mv->str.len;
				utf_parse_blk->scan_char_count = (pcuentmax - 1)->charcnt;
				utf_parse_blk->scan_char_len = 0;		/* Since didn't find char this value not set */
				utf_parse_blk->scan_char_type = UTFCGR_EOL;
				DUMP_UTFCACHE_END(mv, utfcgrp);
				return FALSE;
			}
			/* Case 2b - Set up for continued scan below. The current utfcgrep group is pointing to a new group entry.
			 * Move it back 1 to point to the actual last group - at least temporarily. Note this also means a
			 * temporary discontinuity between the scan variables tbyteidx and tcharcnt that is fixed (if needbe)
			 * in the follow-on code paragraph.
			 *
			 * First check if we would be adding chars to the previous scan group or starting a new group, or, finally,
			 * if all of the scan groups are used with no ability to save more. In this case, we use a different
			 * scan as noted below that runs faster without all the group saving work.
			 */
			utfcgrep_prev = utfcgrep - 1;
			scanptr = (unsigned char *)mv->str.addr + utfcgrep_prev->byteidx;
			assert(scanptr < scantop);
			UTF_CHARTYPELEN(scanptr, scantop, chartype, bytecnt);
			/* Check type of last char in last cache group to the type of the next character to scan. If there's a
			 * match, we can add it to the old group if the type is not UTF8, or if it is UTF8, then if there are
			 * fewer than UTFCGR_MAX_UTF_LEN chars in the group. Else, we need to start a new group.
			 */
			COUNT_UTF_EVENT(parscan);			/* Count partial scans we do */
			if ((chartype == (lchar_typflags = utfcgrep_prev->typflags))	/* Note assignment */
			    && ((UTFCGR_UTF != lchar_typflags)
				|| (UTFCGR_MAX_UTF_LEN >
				    (utfcgrep_prev->charcnt - ((utfcgrep_prev != utfcgrep0) ? (utfcgrep_prev - 1)->charcnt : 0)))))
			{	/* We can add chars to the previous last group - Restore scanning vars as if still in last group */
				DBGUTFC((stderr, "  utfcgr_scanforcharN: Adding newly scanned char to current group (type %d)\n",
					 chartype));
				utfcgrep = utfcgrep_prev;		/* Fix things up to go back to prev group */
				if (utfcgrep != utfcgrep0)
				{	/* This is not the first entry[] so we can address the previous entry and set up our vars
					 * as if the last group had not yet been terminated.
					 */
					tbyteidx = (utfcgrep - 1)->byteidx;
					tcharcnt = (utfcgrep - 1)->charcnt;
					gbytecnt = utfcgrep->byteidx - tbyteidx;
					gcharcnt = utfcgrep->charcnt - tcharcnt;
				} else
				{	/* This is the first entry, use initial values */
					tbyteidx = tcharcnt = 0;
					gbytecnt = utfcgrep->byteidx;
					gcharcnt = utfcgrep->charcnt;	/* Start scan with chars already processed */
					lchar_typflags = utfcgrep->typflags;
				}
			} else
			{	/* Else, we are starting a new group - if one is available. The values are already initialized
				 * correctly for the new group so fall into the (re)start of the scan after bumping relevant
				 * counter(s). The one caveat is if there are no more slots available then we will fall into
				 * a different (faster) scanner that doesn't try to save cache entries
				 */
				gbytecnt = 0;
				gcharcnt = 0;				/* No chars added to this entry yet */
				if (utfcgrep < pcentmax)
				{	/* Account for new group when scanner loop went past pcuentmax above leaving
					 * utfcgrep pointing to a new (unused) group.
					 */
					utfcgrp->ngrps++;
					DBGUTFC((stderr, "  utfcgr_scanforcharN: NEW group for string 0x"lvaddr
						 " (type %d) changing group count from %d to %d\n",
						 mv->str.addr, chartype, utfcgrp->ngrps - 1, utfcgrp->ngrps));
					utfcgrep->typflags = lchar_typflags = chartype;
				} else
				{
					DBGUTFC((stderr, "  utfcgr_scanforcharN: No more group entries available\n"));
				}
			}
		} else
		{	/* Cache was not validated - start from scratch (note tcharcnt and tbyteidx already initialized above) */
			COUNT_UTF_EVENT(miss);
			utfcgrp = utfcgr_getcache(utf_parse_blk->mv);
			utfcgrep = utfcgrep0 = &utfcgrp->entry[0];	/* Point to first entry to add */
			pcentmax = &utfcgrp->entry[TREF(gtm_utfcgr_string_groups)]; /* Pointer to last entry + 1 in this row */
			utfcgrp->reference = TRUE;
			scanptr = (unsigned char *)mv->str.addr;
			lchar_typflags = UTFCGR_NONE;			/* No type set yet */
			gbytecnt = 0;
			gcharcnt = 0;					/* No chars added to this entry yet */
			bytecnt = 0;
		}
		/* (Re)Start the scan filling in scan blocks as we go. If we run out of scan blocks to fill in, drop down to the
		 * scan that does not care about filling in blocks as it runs faster without the overhead.
		 */
		if (utfcgrep < pcentmax)
		{	/* We have additional groups to use - fill them in as we either scan the string for the first time or
			 * pick up where we left off last time.
			 */
			DBGUTFC((stderr, "  utfcgr_scanforcharN: Entering main scan/parse\n"));
			noslots = cachemod = FALSE;
			for (; (scanptr < scantop) && (char_idx > (tcharcnt + gcharcnt));)
			{	/* Scan one char at a time determining both type and length of each character */
				cachemod = TRUE;				/* We are actually adding to the scan */
				UTF_CHARTYPELEN(scanptr, scantop, chartype, bytecnt);
				DBGUTFC((stderr, "  utfcgr_scanforcharN: New char scanned - type: %d, current group type: %d\n",
					 chartype, lchar_typflags));
				if ((UTFCGR_BADCHAR == chartype) && utf_parse_blk->stoponbadchar)
				{	/* Found BADCHAR in the string and this scan can't tolerate them */
					DBGUTFC((stderr, "  utfcgr_scanforcharN: Returning due to BADCHAR\n"));
					utf_parse_blk->badcharstr = scanptr;
					utf_parse_blk->badchartop = scantop;
					utf_parse_blk->scan_char_type = UTFCGR_BADCHAR;
					DUMP_UTFCACHE_END(mv, utfcgrp);
					return FALSE;
				}
				if ((chartype != lchar_typflags)
				    || ((UTFCGR_UTF == lchar_typflags) && (UTFCGR_MAX_UTF_LEN < gcharcnt)))
				{	/* Has the appearance of a change in character type or UTF8 len exceeded- check it */
					if (UTFCGR_NONE == lchar_typflags)
					{	/* First entry in a group so just set it and let it rip */
						utfcgrep->typflags = lchar_typflags = chartype;
						DBGUTFC((stderr, "  utfcgr_scanforcharN: Starting first group with type %d\n",
							 chartype));
					} else
					{	/* We have had a character type change - close out current group and advance to the
						 * next group if one is available.
						 */
						DBGUTFC((stderr, "  utfcgr_scanforcharN: Character type change - old type %d, new "
							 "type %d at string offset %d\n",
							 lchar_typflags, chartype, tbyteidx + gbytecnt));
						DBGUTFC((stderr, "  utfcgr_scanforcharN: Closing group for string 0x"lvaddr
							 " (oldtype %d type %d) with group count %d\n",
							 mv->str.addr, utfcgrep->typflags, lchar_typflags, utfcgrp->ngrps));
						assert((scanptr - (unsigned char *)mv->str.addr) == (tbyteidx + gbytecnt));
						utfcgrep->byteidx = tbyteidx = tbyteidx + gbytecnt;
						utfcgrep->charcnt = tcharcnt = tcharcnt + gcharcnt;
						assert(lchar_typflags == utfcgrep->typflags);
						gbytecnt = gcharcnt = 0;
						if ((utfcgrep + 1) >= pcentmax)
						{	/* No more slots available - fall to simpler scan */
							noslots = TRUE;
							DBGUTFC((stderr, "  utfcgr_scanforcharN: Last char group full - resorting "
								 "to regular scan\n"));
							break;
						}
						DBGUTFC((stderr, "  utfcgr_scanforcharN: New group for string 0x"lvaddr
							 " (type %d) changing group count from %d to %d\n",
							 mv->str.addr, chartype, utfcgrp->ngrps, utfcgrp->ngrps + 1));
						utfcgrp->ngrps++;
						utfcgrep++;
						assert(utfcgrp->ngrps == ((utfcgrep - utfcgrep0) + 1));
						utfcgrep->typflags = lchar_typflags = chartype;	/* Slot available - store type */
					}
				}
				/* Bump what needs bumping depending on character types */
				gcharcnt++;				/* Count char for this group */
				switch(chartype)
				{
					case UTFCGR_ASCII:		/* Both of these types are single-byte chars */
					case UTFCGR_BADCHAR:
						scanptr++;
						gbytecnt++;
						break;
					case UTFCGR_UTF:		/* Multi-byte chars */
						scanptr += bytecnt;
						gbytecnt += bytecnt;
						break;
					default:
						assertpro(FALSE);
				}
				DBGUTFC((stderr, "  utfcgr_scanforcharN: Bottom of scan loop after increment - tcharcnt: %d  "
					 "gcharcnt: %d  tbyteidx: %d  gbytecnt: %d\n", tcharcnt, gcharcnt, tbyteidx, gbytecnt));
			}
			DBGUTFC((stderr, "  utfcgr_scanforcharN: Scan loop exit - cache modded flag: %d\n", cachemod));
			if (!noslots)
			{	/* We didn't exit loop for lack of slots so we either reached our goal or ran out of text to parse.
				 * Either one means we need to close the last group we were scanning and return either success
				 * or failure. But before we close it, we can add the character to the group that we're looking
				 * at (if there is one) and are going to return the position of to the buffer.
				 */
				assert((scanptr - (unsigned char *)mv->str.addr) == (tbyteidx + gbytecnt));
				lchar_byteidx = tbyteidx + gbytecnt;	/* Record info changed when fetch info on return char
									 * and put it in cache.
									 */
				lchar_charcnt = tcharcnt + gcharcnt;
				DEBUG_ONLY(lchar_typflags = chartype);
				DBGUTFC((stderr, "  utfcgr_scanforcharN: End-scan (!noslots) scan vars - lchar_byteidx: %d  "
					 "  lchar_charcnt: %d  lchar_typflags: %d\n", lchar_byteidx, lchar_charcnt,
					 lchar_typflags));
				if (scanptr < scantop)
				{	/* The char to return info on is at lchar_byteidx. It's type is not currently known. There
					 * is more text to scan so see what type/bytelen the character is. If we are able to,
					 * once we determine this info, the char will be added to the cache to make the char easier
					 * to find next time.
					 */
					UTF_CHARTYPELEN(scanptr, scantop, chartype, bytecnt);	/* Get info on char to return */
					DBGUTFC((stderr, "  utfcgr_scanforcharN: scanning end char to add it to cache - new char"
						 " type: %d, current type: %d, utfcgerp: 0x" lvaddr" - group: %d\n",
						 chartype, utfcgrep->typflags, utfcgrep, (utfcgrep - utfcgrep0) + 1));
					if ((chartype == utfcgrep->typflags) && ((scanptr + bytecnt) <= scantop))
					{	/* Char is same type and within same buffer - add to existing group */
						scanptr += bytecnt;
						gcharcnt++;
						gbytecnt += bytecnt;
						cachemod = TRUE;	/* Adding chars to cache */
						DBGUTFC((stderr, "  utfcgr_scanforcharN: char is same type as previous char - "
							 "gbytecnt: %d, gcharcnt: %d, bytecnt: %d\n", gbytecnt, gcharcnt, bytecnt));
					} else
					{	/* Must create new group for this character to reside in if there is room */
						if ((UTFCGR_BADCHAR == chartype) && (utf_parse_blk->stoponbadchar))
						{	/* But we have a BADCHAR we must report */
							DBGUTFC((stderr, "  utfcgr_scanforcharN: Returning due to BADCHAR\n"));
							/* Close the open group before return error */
							utfcgrep->byteidx = tbyteidx + gbytecnt;
							utfcgrep->charcnt = tcharcnt + gcharcnt;
							utf_parse_blk->badcharstr = scanptr;
							utf_parse_blk->badchartop = scantop;
							utf_parse_blk->scan_char_type = UTFCGR_BADCHAR;
							DUMP_UTFCACHE_END(mv, utfcgrp);
							return FALSE;
						}
						DBGUTFC((stderr, "  utfcgr_scanforcharN: character type change - old type %d, new "
							 "type %d at string offset %d\n",
							 utfcgrep->typflags , chartype, tbyteidx + gbytecnt));
						if ((utfcgrep + 1) < pcentmax)
						{	/* There is room - close old entry and allocate a new entry */
							utfcgrep->byteidx = tbyteidx = tbyteidx + gbytecnt;
							utfcgrep->charcnt = tcharcnt = tcharcnt + gcharcnt;
							assert(utfcgrep->typflags == lchar_typflags);
							DBGUTFC((stderr, "  utfcgr_scanforcharN: new group for string 0x"lvaddr
								 " (type %d) changing group count from %d to %d\n",
								 mv->str.addr, chartype, utfcgrp->ngrps, utfcgrp->ngrps + 1));
							utfcgrep++;
							utfcgrp->ngrps++;
							assert(utfcgrp->ngrps == ((utfcgrep - utfcgrep0) + 1));
							utfcgrep->typflags = chartype;	/* Init type of new group */
							gcharcnt = 1;
							gbytecnt = bytecnt;
							cachemod = TRUE;	/* Adding chars to cache */
						} else
						{	/* Else - no room to store anything. Note if in debug mode, we need
							 * to reset chartype back to lchar_typflags since we were unable to
							 * cache the new char or create a new group for it. This allows later
							 * asserts to work correctly.
							 */
							DBGUTFC((stderr, "  utfcgr_scanforcharN: no room to create a new group -"
								 " new char not added to cache\n"));
							DEBUG_ONLY(chartype = lchar_typflags);
						}
					}
					scaneol = FALSE;
				} else
				{	/* We ran out of characters to scan - flag it */
					scaneol = TRUE;
					DBGUTFC((stderr, "  utfcgr_scanforcharN: Scan ended due to EOL\n"));
				}
				/* Cases:
				 *
				 *   a. We hit the end of the scan by running out of text to parse so chartype is unchanged.
				 *   b. We looked at the char to return and it was the same type as the current group so char
				 *      is added to that group so chartype is again unchanged.
				 *   c. We looked at the char to return and it was a different type from the current group so we
				 *      created a new group to hold the new char so chartype has changed.
				 *   d. As in (c) but there were no new utfcgr_entrys available so no change to cache.
				 *
				 * If the cache was modified, close the last entry. This could be the entry last created by the
				 * main scan loop or it could be a new group created to hold the char we are returning info about.
				 */
				if (cachemod)
				{	/* We made some changes to the cache so close the current entry out */
					DBGUTFC((stderr, "  utfcgr_scanforcharN: changes made to cache this pass - close current"
						 " group\n"));
					utfcgrep->byteidx = tbyteidx + gbytecnt;
					utfcgrep->charcnt = tcharcnt + gcharcnt;
					DBGUTFC((stderr, "  utfcgr_scanforcharN: closing group 0x"lvaddr" for string 0x"lvaddr
						 " (oldtype %d type %d) with group count %d - byteidx: %d  charcnt: %d\n",
						 utfcgrep, mv->str.addr, utfcgrep->typflags, chartype, utfcgrp->ngrps,
						 utfcgrep->byteidx, utfcgrep->charcnt));
					assert(utfcgrep->typflags == (scaneol ? lchar_typflags : chartype));
				} else
				{	/* This was just a "read" of existing cache - no updates necessary */
					DBGUTFC((stderr, "  utfcgr_scanforcharN: readonly use of cache - nothing to see here -"
						 " move along\n"));
				}
				/* Fill in output values */
				utf_parse_blk->scan_byte_offset = lchar_byteidx;
				utf_parse_blk->scan_char_count = lchar_charcnt;
				if (!scaneol && (char_idx == lchar_charcnt))
				{	/* We found our char */
					utf_parse_blk->scan_char_len = bytecnt;
					utf_parse_blk->scan_char_type = chartype;
					utf_parse_blk->utfcgr_indx = utfcgrp->ngrps - 1; /* Save in utf_parse_blk as zero origin */
					DUMP_UTFCACHE_END(mv, utfcgrp);
					assert((mv->str.addr + utf_parse_blk->scan_byte_offset + utf_parse_blk->scan_char_len)
					       <= (char *)scantop);	/* Verify entire char we pass back exists within string */
					return TRUE;
				} else
				{	/* The char was not found */
					utf_parse_blk->scan_char_len = 0;
					utf_parse_blk->utfcgr_indx = -1;		/* Bogus return value when not found */
					utf_parse_blk->scan_char_type = UTFCGR_EOL;
					DUMP_UTFCACHE_END(mv, utfcgrp);
					return FALSE;
				}
			}
			/* This is when we are leaving the cache area but are continuing with the brute force scan */
			DUMP_UTFCACHE_END(mv, utfcgrp);
		}
		/* Falling through to here means to continue the scan without saving any group entries because we ran out
		 * of slots to put them in. We use the same scan as if there was no cache involved at all.
		 */
		COUNT_UTF_EVENT(parhscan);
		skip = char_idx - tcharcnt;				/* Bytes left to scan to find target char */
	} else
	{	/* This string is too small to use cache on - do the simple scan in brute force mode */
		COUNT_UTF_EVENT(small);
		scanptr = (unsigned char *)mv->str.addr + utf_parse_blk->scan_byte_offset;
		if (0 < utf_parse_blk->scan_byte_offset)
		{	/* If an offset was specified, pickup the char offset we started looking at */
			tcharcnt = utf_parse_blk->scan_char_count;
			assert(0 < tcharcnt);
			skip = char_idx - tcharcnt;
		} else
		{
			tcharcnt = 0;
			skip = char_idx;
		}
	}
	assert(0 <= skip);
	/* This scan is used two ways:
	 *
	 * 1. When the string is too small to have a cache associated with it.
	 * 2. When the cache for a given string is full so all additional scanning needs to be as fast as possible.
	 *
	 * The code expects the following vars setup:
	 *
	 *   scanptr       - Points to characters to scan.
	 *   scantop       - Points to end of scanning chars + 1.
	 *   skip          - The number of chars to skip before returning the "next" character.
	 *   utf_parse_blk - Pointer to parse block where results are stored for return to user.
	 */
	DBGUTFC((stderr, "  utfcgr_scanforcharN: Begin non-cacheing scan with skip=%d\n", skip));
	lastcharbad = FALSE;
	for (; (0 < skip) && (scanptr < scantop); skip--, scanptr += bytecnt, tcharcnt++)
	{	/* Advance the string to locate the desired character */
		if ((!UTF8_VALID(scanptr, scantop, bytecnt)) && utf_parse_blk->stoponbadchar)
		{	/* Found BADCHARs in the string and this scan can't tolerate them */
			utf_parse_blk->badcharstr = scanptr;
			utf_parse_blk->badchartop = scantop;
			utf_parse_blk->scan_char_type = UTFCGR_BADCHAR;
			return FALSE;
		}
	}
	/* If we found the character, return it (or BADCHAR indicator) - else return char-not-found */
	utf_parse_blk->scan_char_count = tcharcnt;
	if ((0 == skip) && (scanptr < scantop))
	{	/* Character position was found */
		assert(tcharcnt == char_idx);
		if ((lastcharbad = !UTF8_VALID(scanptr, scantop, bytecnt)) && utf_parse_blk->stoponbadchar) /* Note assignment */
		{	/* Spotted a BADCHAR and we aren't tolerating those at this time */
			utf_parse_blk->badcharstr = scanptr;
			utf_parse_blk->badchartop = scantop;
			utf_parse_blk->scan_char_type = UTFCGR_BADCHAR;
			return FALSE;
		}
		utf_parse_blk->scan_byte_offset = scanptr - (unsigned char *)mv->str.addr;
		utf_parse_blk->scan_char_len = bytecnt;
		utf_parse_blk->scan_char_type = (1 != bytecnt) ? UTFCGR_UTF : (lastcharbad ? UTFCGR_BADCHAR : UTFCGR_ASCII);
		utf_parse_blk->utfcgr_indx = -1;					/* No cache, no index */
		return TRUE;
	} else
	{	/* Character position was not found - no usable values returned */
		utf_parse_blk->scan_byte_offset = mv->str.len;
		/* Note utf_parse_blk->scan_char_count already set above */
		utf_parse_blk->scan_char_len = 0;
		utf_parse_blk->scan_char_type = UTFCGR_EOL;
		return FALSE;
	}
}

#ifdef DEBUG
void utfcgr_stats(void)
{
	FPRINTF(stderr, "\nUTF cache hits:                                    %d\n", u_hit);
	FPRINTF(stderr, "UTF cache misses:                                  %d\n", u_miss);
	FPRINTF(stderr, "UTF brute force string scans:                      %d\n", u_small);
	FPRINTF(stderr, "Number of groups skipped                           %d\n", u_pskip);
	FPRINTF(stderr, "Number of UTF groups scanned for located char:     %d\n", u_puscan);
	FPRINTF(stderr, "Number of non-UTF groups scanned for located char: %d\n", u_pabscan);
	FPRINTF(stderr, "Number of partial scans (partial cache hits):      %d\n", u_parscan);
	FPRINTF(stderr, "Number of partial scans after filled slots:        %d\n", u_parhscan);
}
#endif	/* DEBUG */
#endif	/* UNICODE_SUPPORTED */
