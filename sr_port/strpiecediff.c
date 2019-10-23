/****************************************************************
 *								*
 *	Copyright 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "strpiecediff.h"
#include "matchc.h"
#include "stringpool.h"

#define	PIECEDIFF_START_BUFSIZ	64

#define	OLD_END_SEEN	(1 << 0)
#define	NEW_END_SEEN	(1 << 1)
#define	NO_END_SEEN	0
#define	BOTH_END_SEEN	(OLD_END_SEEN | NEW_END_SEEN)

#define	SKIP_PIECES(DELIM_LEN, DELIM_PTR, LEN, PTR, END, TOP, SKIPPCS, END_SEEN, MASK)				\
{														\
	int	match_res, skppcs;										\
														\
	if (LEN)												\
	{													\
		skppcs = SKIPPCS; /* store in local so input parameter is untouched */				\
		END = (char *)(*match_fn)(DELIM_LEN, DELIM_PTR, LEN, (unsigned char *)PTR, &match_res, &skppcs);\
		if (0 != match_res)										\
		{												\
			LEN -= INTCAST(END - PTR);								\
			PTR = END;										\
			assert(PTR + LEN == TOP);								\
		} else												\
		{												\
			LEN = 0; /* Could not skip all specified # of pieces ("skippcs") */			\
			END_SEEN |= MASK;									\
		}												\
	} else													\
		END_SEEN |= MASK;										\
}

#define	MATCH_CURPIECE(DELIM_LEN, DELIM_PTR, LEN, PTR, MATCH_LEN, END, END_SEEN, MASK, PIECE_EXISTS)		\
{														\
	int	match_res, numpcs;										\
														\
	if (!(MASK & END_SEEN))											\
	{													\
		numpcs = 1;											\
		PIECE_EXISTS = TRUE;										\
		END = (char *)(*match_fn)(DELIM_LEN, DELIM_PTR, LEN, (unsigned char *)PTR, &match_res, &numpcs);\
		MATCH_LEN = INTCAST(END - PTR);									\
		if (0 == match_res)										\
		{												\
			assert(1 == numpcs);									\
			END_SEEN |= MASK;									\
		} else												\
		{												\
			assert((0 == numpcs) && ((PTR + DELIM_LEN) <= END));					\
			MATCH_LEN -= DELIM_LEN;									\
		}												\
	} else													\
	{													\
		PIECE_EXISTS = FALSE;										\
		MATCH_LEN = 0;											\
	}													\
}

#define	UPDATE_PTR_LEN(LEN, PTR, END_SEEN, MASK, END, TOP)	\
{								\
	if (!(MASK & END_SEEN))					\
	{							\
		LEN -= INTCAST(END - PTR);			\
		PTR = END;					\
		assert(PTR + LEN == TOP);			\
	}							\
}

typedef unsigned char * (*match_fnptr)(int del_len, unsigned char *del_str, int src_len,
							unsigned char *src_str, int *res, int *numpcs);

/* Finds the list of pieces which differ between oldstr and newstr using "delim" as the delimiter.
 * An optional piecelist can specify pieces of interest. If NULL, all pieces that differ are returned.
 * use_matchc controls whether byte-oriented or character-oriented match is done.
 * 	If TRUE, matchc() is used. If FALSE, matchb() is used.
 */
void	strpiecediff(mstr *oldstr, mstr *newstr, mstr *delim,
			uint4 numpieces, gtm_num_range_t *piecearray, boolean_t use_matchc, mstr *pcdiff_mstr)
{
	int		curpiece, bufflen, newbufflen, skippcs, save_skippcs;
	int		delim_len, old_len, new_len, match_res, old_match_len, new_match_len, pcdiff_len;
	uint4		nextmin, nextmax, prev_nextmin;
	char		*old_ptr, *old_top, *new_ptr, *new_top, *next_ptr, *old_end, *new_end;
	uchar_ptr_t	delim_ptr;
	static	char	pcdiff_buff[PIECEDIFF_START_BUFSIZ], *pcdiff_start = &pcdiff_buff[0], *pcdiff_top = ARRAYTOP(pcdiff_buff);
	char		*pcdiff_ptr = pcdiff_start, *tmpbuff;
	match_fnptr	match_fn;
	uint4		end_seen;
	boolean_t	old_piece_exists, new_piece_exists;
#	ifdef DEBUG
	uint4		save_numpieces;
	gtm_num_range_t	*save_piecearray;
#	endif

	/* Initialize oldstr related fields */
	assert(NULL != oldstr);
	old_ptr = oldstr->addr;
	old_len = oldstr->len;
	old_top = old_ptr + old_len;
	assert(old_ptr <= old_top);
	/* Initialize newstr related fields */
	assert(NULL != newstr);
	new_ptr = newstr->addr;
	new_len = newstr->len;
	new_top = new_ptr + newstr->len;
	assert(new_ptr <= new_top);
	/* Initialize delim related fields */
	assert(NULL != delim);
	delim_ptr = (uchar_ptr_t)delim->addr;
	delim_len = delim->len;
	assert(0 < delim_len);
	/* Initialize piecelist related fields */
	DEBUG_ONLY(save_numpieces = numpieces;)		/* for debugging purposes */
	DEBUG_ONLY(save_piecearray = piecearray;)	/* for debugging purposes */
	if (0 != numpieces)
	{
		nextmin = piecearray[0].min;
		nextmax = piecearray[0].max;
		assert(nextmin <= nextmax);
		piecearray++;
		numpieces--;
	} else
	{
		nextmin = 1;
		nextmax = MAXPOSINT4;
	}
	DEBUG_ONLY(prev_nextmin = nextmin;)
	/* Compute list of pieces that differ */
	curpiece = 1;
	match_fn = use_matchc ? matchc : matchb;
	end_seen = NO_END_SEEN;	/* end has not been seen in either old nor new */
	for ( ; BOTH_END_SEEN != end_seen; )
	{
		assert(nextmin >= curpiece);
		skippcs = nextmin - curpiece;
		if (skippcs)
		{	/* Scanpoint is currently at piece # "curpiece" but user has specified that the next piece of interest
			 * is "nextmin". So skip processing all intermediate pieces in both old and new strings.
			 */
			DEBUG_ONLY(save_skippcs = skippcs;)
			SKIP_PIECES(delim_len, delim_ptr, old_len, old_ptr, old_end, old_top, skippcs, end_seen, OLD_END_SEEN);
			assert(save_skippcs == skippcs);
			SKIP_PIECES(delim_len, delim_ptr, new_len, new_ptr, new_end, new_top, skippcs, end_seen, NEW_END_SEEN);
			assert(save_skippcs == skippcs);
			if (BOTH_END_SEEN == end_seen)
				break;
		}
		curpiece = nextmin;
		MATCH_CURPIECE(delim_len, delim_ptr, old_len, old_ptr, old_match_len, old_end, end_seen,
				OLD_END_SEEN, old_piece_exists);
		MATCH_CURPIECE(delim_len, delim_ptr, new_len, new_ptr, new_match_len, new_end, end_seen,
				NEW_END_SEEN, new_piece_exists);
		if ((FALSE == old_piece_exists) && (FALSE == new_piece_exists))
			break;	/* both pieces do not exist. no more diff possible. break out */
		else if ((old_piece_exists != new_piece_exists)
				|| (old_match_len != new_match_len) || (0 != memcmp(old_ptr, new_ptr, old_match_len)))
		{	/* Pieces differ. Add "curpiece" to list of differing pieces. First check if the piece # can
			 * be stored in the remaining buffer (use maximum possible piece # for this calculation).
			 */
			if (pcdiff_top < (pcdiff_ptr + MAX_DIGITS_IN_INT + 1))	/* + 1 is for a potential ',' */
			{	/* No space to store next piece #. Expand buffer. */
				assert(pcdiff_ptr != &pcdiff_buff[0]);
				bufflen = INTCAST(pcdiff_top - pcdiff_start);
				newbufflen = bufflen * 2;
				tmpbuff = (char *)malloc(newbufflen); /* expand by doubling buffer size */
				memcpy(tmpbuff, pcdiff_start, bufflen);
				if (pcdiff_start != &pcdiff_buff[0])
					free(pcdiff_start);	/* obtained by malloc. so free it */
				pcdiff_len = INTCAST(pcdiff_ptr - pcdiff_start);
				pcdiff_start = tmpbuff;
				pcdiff_ptr = tmpbuff + pcdiff_len;
				pcdiff_top = tmpbuff + newbufflen;
			}
			assert(pcdiff_top >= (pcdiff_ptr + MAX_DIGITS_IN_INT + 1));
			if (pcdiff_ptr != pcdiff_start)
				*pcdiff_ptr++ = ',';	/* Not the first piece in this list so prefix piece # with a ',' */
			I2A_INLINE(pcdiff_ptr, curpiece);
			assert(pcdiff_ptr > pcdiff_start);
			assert(pcdiff_ptr <= pcdiff_top);
		}
		UPDATE_PTR_LEN(old_len, old_ptr, end_seen, OLD_END_SEEN, old_end, old_top);
		UPDATE_PTR_LEN(new_len, new_ptr, end_seen, NEW_END_SEEN, new_end, new_top);
		curpiece++;
		assert(nextmin <= nextmax);
		if (nextmin == nextmax)
		{	/* Find next range */
			if (0 == numpieces)
				break;
			nextmin = piecearray[0].min;
			nextmax = piecearray[0].max;
			piecearray++;
			numpieces--;
			assert(prev_nextmin < nextmin);
			DEBUG_ONLY(prev_nextmin = nextmin;)
		} else
			nextmin++;
	}
	pcdiff_mstr->len = INTCAST(pcdiff_ptr - pcdiff_start);
	pcdiff_mstr->addr = pcdiff_start;
	assert(',' != *pcdiff_start);
	return;
}
