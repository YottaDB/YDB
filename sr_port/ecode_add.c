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

#include "mdef.h"

#include "gtm_string.h"		/* for memcpy() */

#include "min_max.h"		/* for MIN macro */
#include <rtnhdr.h>		/* for stack_frame.h */
#include "stack_frame.h"	/* for stack_frame type */
#include "error_trap.h"

#include "get_command_line.h"	/* for get_command_line() prototype */
#include "dollar_zlevel.h"	/* for dollar_zlevel() prototype */

GBLREF	dollar_ecode_type	dollar_ecode;			/* structure containing $ECODE related information */
GBLREF	dollar_stack_type	dollar_stack;			/* structure containing $STACK related information */
GBLREF	stack_frame		*error_frame;			/* "frame_pointer" at the time of adding the current ECODE */
GBLREF	stack_frame		*frame_pointer;

#define		INCR_ECODE_INDEX(ecode_index, str, strlen)				\
{											\
	memcpy(dollar_ecode.end, str, strlen);						\
	dollar_ecode.array[ecode_index].ecode_str.addr = dollar_ecode.end;		\
	dollar_ecode.array[ecode_index].ecode_str.len = strlen;				\
	/* -1 below for not calculating the terminating ',' as part of this ECODE, 	\
	 * but instead calculate that as part of the beginning of the next ECODE */	\
	dollar_ecode.end += strlen - 1;							\
	ecode_index++;									\
}

#define		DECR_ECODE_INDEX(ecode_index)						\
{											\
	ecode_index--;									\
	space_left += dollar_ecode.array[ecode_index].ecode_str.len - 1;		\
}

/* returns TRUE if able to fit in string held by tmpmval into dollar_stack
 * returns FALSE otherwise */
static	boolean_t	fill_dollar_stack_info(mval *mvalptr, mstr *mstrptr)
{
	ssize_t		space_left;

	if (mvalptr->str.len)
	{
		space_left = dollar_stack.top - dollar_stack.end;
		if (mvalptr->str.len > space_left)
		{
			dollar_stack.incomplete = TRUE; /* we stop storing $STACK(level) info once we reach a frame level
							 * that can't be fitted in the available space */
			return FALSE;
		}
		memcpy(dollar_stack.end, mvalptr->str.addr, mvalptr->str.len);
		mstrptr->addr = dollar_stack.end;
		mstrptr->len = mvalptr->str.len;
		dollar_stack.end += mstrptr->len;
		assert(dollar_stack.end <= dollar_stack.top);
	} else
		mstrptr->len = 0;
	return TRUE;
}

/* returns TRUE if able to fit in one $STACK(level,...) of information in global variable structure "dollar_stack".
 * returns FALSE otherwise */
static	boolean_t	fill_dollar_stack_level(int array_level, int frame_level, int cur_zlevel)
{
	mstr			*mstrptr;
	mval			tmpmval;
	dollar_stack_struct	*dstack;

	assert(FALSE == dollar_stack.incomplete);	/* we should not have come here if previous $STACK levels were incomplete */
	dstack = &dollar_stack.array[array_level];
	/* fill in $STACK(level) */
	if (frame_level)
		get_frame_creation_info(frame_level, cur_zlevel, &tmpmval);
	else
		get_command_line(&tmpmval, FALSE);	/* FALSE to indicate we want actual (not processed) command line */
	/* note that tmpmval at this point will most likely point to the stringpool. but we rely on stp_gcol to free it up */
	mstrptr = &dstack->mode_str;
	if (FALSE == fill_dollar_stack_info(&tmpmval, mstrptr))
		return FALSE;
	/* fill in $STACK(level,"ECODE") */
	dstack->ecode_ptr = (frame_level == (cur_zlevel - 1)) ? &dollar_ecode.array[dollar_ecode.index - 1] : NULL;

	/* fill in $STACK(level,"PLACE") */
	get_frame_place_mcode(frame_level, DOLLAR_STACK_PLACE, cur_zlevel, &tmpmval);
	mstrptr = &dstack->place_str;
	if (FALSE == fill_dollar_stack_info(&tmpmval, mstrptr))
		return FALSE;
	/* fill in $STACK(level,"MCODE") */
	get_frame_place_mcode(frame_level, DOLLAR_STACK_MCODE, cur_zlevel, &tmpmval);
	mstrptr = &dstack->mcode_str;
	if (FALSE == fill_dollar_stack_info(&tmpmval, mstrptr))
		return FALSE;
	return TRUE;
}

boolean_t	ecode_add(mstr *str)		/* add "str" to $ECODE and return whether SUCCESS or FAILURE as TRUE/FALSE */
{
	int		ecode_index, stack_index;
	boolean_t	shrink;
 	int		cur_zlevel, level;
	char		eclostmid_buf[MAX_DIGITS_IN_INT + STR_LIT_LEN(",Z,")], *dest;
	ssize_t		space_left, eclostmid_len;

	error_def(ERR_ECLOSTMID);

	dest = &eclostmid_buf[0];
	*dest++ = ',';
	*dest++ = 'Z';
	dest = (char *)i2asc((unsigned char *)dest, ERR_ECLOSTMID);
	*dest++ = ',';
	eclostmid_len = dest - &eclostmid_buf[0];
	assert(SIZEOF(eclostmid_buf) >= eclostmid_len);

	assert(str->len < DOLLAR_ECODE_ALLOC);
	space_left = dollar_ecode.top - dollar_ecode.end;
	ecode_index = dollar_ecode.index;
	shrink = FALSE;
	if (space_left < str->len)
	{
		shrink = TRUE;
		assert(1 == shrink); 	/* since we need a value of 1 (instead of any non-zero) for usage below */
		space_left -= eclostmid_len - 1;/* note : space_left can become negative but code below handles that */
	}
	if (ecode_index >= (DOLLAR_ECODE_MAXINDEX - shrink))
	{
		assert(DOLLAR_ECODE_MAXINDEX >= ecode_index);
		if (DOLLAR_ECODE_MAXINDEX == ecode_index)
		{
			DECR_ECODE_INDEX(ecode_index);
			shrink = TRUE;
		}
		if (shrink)
		{
			DECR_ECODE_INDEX(ecode_index);
			assert((DOLLAR_ECODE_MAXINDEX - 2) == ecode_index);
		}
	}
	assert(ecode_index < DOLLAR_ECODE_MAXINDEX);
	for ( ; space_left < (int)str->len; )	/* note explicit typecasting to make sure it is a signed comparison */
	{
		ecode_index--;
		if (1 > ecode_index)	/* if ecode_index == -1 ==> str->len > DOLLAR_ECODE_ALLOC so nothing can be done in PRO */
			return FALSE;	/* if ecode_index == 0 ==> first ECODE needs to be overlaid. we do not want to do that. */
		space_left += dollar_ecode.array[ecode_index].ecode_str.len - 1;
	}
	for (stack_index = 0; stack_index < dollar_stack.index; stack_index++)
	{
		if (dollar_stack.array[stack_index].ecode_ptr > &dollar_ecode.array[ecode_index])
			return FALSE;	/* do not want to overlay any ECODE that $STACK(level,"ECODE") is pointing to */
	}
	assert(0 <= ecode_index);
	if (dollar_ecode.index != ecode_index)
	{
		dollar_ecode.end = dollar_ecode.array[ecode_index].ecode_str.addr;
		dollar_ecode.index = ecode_index;
	}
	if (shrink)
	{
		INCR_ECODE_INDEX(dollar_ecode.index, &eclostmid_buf[0], (mstr_len_t)eclostmid_len);
	}
	INCR_ECODE_INDEX(dollar_ecode.index, str->addr, str->len);
	if ((1 == dollar_ecode.index)
			|| ((!dollar_stack.incomplete) && (2 == dollar_ecode.index)
				&& (dollar_ecode.first_ecode_error_frame == error_frame)))
	{	/* need to fill in $STACK entries if either the first ECODE or if an error in the first ECODE error-handler.
		 * do not fill in nested error $STACK info if the first ECODE's $STACK info itself was incompletely filled in */
		if (1 == dollar_ecode.index)
		{	/* first ECODE. note down error_frame info in "first_ecode_error_frame" as well as $STACK(level) info */
			dollar_ecode.first_ecode_error_frame = frame_pointer;
			assert(0 == dollar_stack.index);
		}
		cur_zlevel = dollar_zlevel();
		assert(dollar_stack.index <= cur_zlevel);
		for (level = dollar_stack.index; level < MIN(cur_zlevel, DOLLAR_STACK_MAXINDEX); )
		{	/* we do not store $STACK(level) info for levels > 256 */
			if (fill_dollar_stack_level(level, level, cur_zlevel))
				level++;	/* update array_level only if we had enough space to fill in all of above */
			else
				break;
		}
		if ((2 == dollar_ecode.index) && (cur_zlevel == dollar_stack.index) && (DOLLAR_STACK_MAXINDEX > cur_zlevel))
		{	/* if nested error occurred at the same frame_level as the first error,
			 * store $STACK information for the nested error in $STACK(frame_level+1)
			 */
			assert(level == dollar_stack.index);
			if (fill_dollar_stack_level(level, cur_zlevel - 1, cur_zlevel))
				level++;
		}
		dollar_stack.index = level;
	}
	return TRUE;
}
