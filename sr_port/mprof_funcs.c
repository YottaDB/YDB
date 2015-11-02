/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#if defined(UNIX)
#include "gtm_stdio.h"
#include <errno.h>
#include "gtm_fcntl.h"
#include <signal.h>
#include "gtm_stat.h"
#include "gtm_unistd.h"
#include "min_max.h"
#elif defined(VMS)
#include <ssdef.h>
#include <descrip.h>
#include <jpidef.h>
#endif

#include "gtm_ctype.h"
#include "gtm_string.h"
#include "error.h"
#include "rtnhdr.h"
#include "stack_frame.h"
#include "subscript.h"
#include "svnames.h"
#include "mprof.h"
#include "outofband.h"
#include "op.h"
#include "lv_val.h"	/* needed for "callg.h" */
#include "callg.h"
#include "gtmmsg.h"
#include "str2gvargs.h"

GBLREF 	boolean_t		is_tracing_on;
GBLREF 	stack_frame		*frame_pointer;
GBLREF 	mval			dollar_job;
GBLREF	uint4			process_id;
GBLREF	int * volatile		var_on_cstack_ptr;	/* volatile so that nothing gets optimized out */
GBLREF	int4			gtm_trigger_depth;

LITDEF  MIDENT_CONST(above_routine, "*above*");

#define MPROF_NULL_LABEL "^"
#define MPROF_FOR_LOOP	"FOR_LOOP"
#define UPDATE_TIME(x)	x->e.usr_time += ((TREF(mprof_ptr))->tcurr.tms_utime - (TREF(mprof_ptr))->tprev.tms_utime);\
			x->e.sys_time += ((TREF(mprof_ptr))->tcurr.tms_stime - (TREF(mprof_ptr))->tprev.tms_stime);
#define RTS_ERROR_VIEWNOTFOUND(x)	rts_error(VARLSTCNT(8) ERR_VIEWNOTFOUND, 2, gvn->str.len, gvn->str.addr, \
						ERR_TEXT, 2, RTS_ERROR_STRING(x));

#ifdef UNIX
#define TIMES			times_usec
#elif defined(VMS)
#define TIMES			get_cputime
#endif

error_def(ERR_MAXNRSUBSCRIPTS);
error_def(ERR_MAXTRACELEVEL);
error_def(ERR_NOTGBL);
error_def(ERR_STRUNXEOR);
error_def(ERR_SYSCALL);
error_def(ERR_TEXT);
error_def(ERR_TRACINGON);
error_def(ERR_VIEWNOTFOUND);

STATICFNDCL void parse_gvn(mval *);
STATICFNDCL void get_entryref_information(boolean_t, trace_entry *);

#ifdef UNIX
STATICFNDCL void times_usec(struct tms *curr);
STATICFNDEF void times_usec(struct tms *curr)
{
	int res;
	struct rusage usage;

	res = getrusage(RUSAGE_SELF, &usage);
	if (res == -1)
		rts_error(VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("getrusage"), CALLFROM, errno);
	curr->tms_utime = (usage.ru_utime.tv_sec * 1000000) + usage.ru_utime.tv_usec;
	curr->tms_stime = (usage.ru_stime.tv_sec * 1000000) + usage.ru_stime.tv_usec;
	return;
}

#elif defined(VMS)
STATICFNDCL void get_cputime (struct tms *curr);
STATICFNDEF void get_cputime (struct tms *curr)
{
	int4	cpu_time_used;
	int	status;
	int	jpi_code = JPI$_CPUTIM;

	if ((status = lib$getjpi(&jpi_code, &process_id, 0, &cpu_time_used, 0, 0)) != SS$_NORMAL)
		rts_error(VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("LIB$GETJPI"), CALLFROM, status);
	curr->tms_utime = cpu_time_used;
	curr->tms_stime = 0;
	return;
}
#else
#error UNSUPPORTED PLATFORM
#endif

void turn_tracing_on(mval *gvn)
{
	struct tms	*curr;
	trace_entry	tmp_trc_tbl_entry;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (is_tracing_on)
	{
		gtm_putmsg(VARLSTCNT(1) ERR_TRACINGON);
		return;
	}
	if (0 == gvn->str.len || '^' != gvn->str.addr[0])
		rts_error(VARLSTCNT(4) ERR_NOTGBL, 2, gvn->str.len, gvn->str.addr);
	if (!TREF(mprof_ptr))
	{
		TREF(mprof_ptr) = (mprof_wrapper *)malloc(SIZEOF(mprof_wrapper));
		memset(TREF(mprof_ptr), 0, SIZEOF(mprof_wrapper));
	}
	parse_gvn(gvn);
	(TREF(mprof_ptr))->gbl_to_fill = *gvn;
	(TREF(mprof_ptr))->gbl_to_fill.str.addr = (char *)malloc(gvn->str.len); /* len was already set up */
	memcpy((TREF(mprof_ptr))->gbl_to_fill.str.addr, gvn->str.addr, gvn->str.len);
	if (!(TREF(mprof_ptr))->pcavailbase)
	{
		(TREF(mprof_ptr))->pcavailbase = (char **)malloc(PROFCALLOC_DSBLKSIZE);
		*(TREF(mprof_ptr))->pcavailbase = 0;
	}
	(TREF(mprof_ptr))->pcavailptr = (TREF(mprof_ptr))->pcavailbase;
	(TREF(mprof_ptr))->pcavail = PROFCALLOC_DSBLKSIZE - SIZEOF(char *);
	memset((TREF(mprof_ptr))->pcavailptr + 1, 0, (TREF(mprof_ptr))->pcavail);
	curr = &((TREF(mprof_ptr))->tprev);
	TIMES(curr);
	mprof_stack_init();
	TREF(prof_fp) = mprof_stack_push();
	get_entryref_information(FALSE, NULL);
	tmp_trc_tbl_entry.rout_name = NULL;
	tmp_trc_tbl_entry.label_name = NULL;
	(TREF(mprof_ptr))->curr_tblnd = (TREF(mprof_ptr))->head_tblnd = NULL;
	(TREF(prof_fp))->start.tms_stime = (*curr).tms_stime;
	(TREF(prof_fp))->start.tms_utime = (*curr).tms_utime;
	(TREF(prof_fp))->carryover.tms_stime = 0;
	(TREF(prof_fp))->carryover.tms_utime = 0;
	(TREF(prof_fp))->dummy_stack_count = 0;
	(TREF(prof_fp))->rout_name = (TREF(prof_fp))->label_name = NULL;
	POPULATE_PROFILING_TABLE();
	is_tracing_on = TRUE;
	return;
}

void turn_tracing_off (mval *gvn)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (FALSE == is_tracing_on)
		return;
	assert(TREF(mprof_ptr));
	TIMES(&(TREF(mprof_ptr))->tcurr);
	/* update the time of previous M line, if there was one */
	if (NULL != (TREF(mprof_ptr))->curr_tblnd)
	{
		UPDATE_TIME((TREF(mprof_ptr))->curr_tblnd);
	}
	if (NULL != gvn)
		parse_gvn(gvn);
	is_tracing_on = (TREF(mprof_ptr))->is_tracing_ini = FALSE;
	assert(0 != (TREF(mprof_ptr))->gbl_to_fill.str.addr);
	free((TREF(mprof_ptr))->gbl_to_fill.str.addr);
	(TREF(mprof_ptr))->gbl_to_fill.str.addr = NULL;
	mprof_tree_walk((TREF(mprof_ptr))->head_tblnd);
	mprof_stack_free();
	(TREF(mprof_ptr))->pcavailptr = (TREF(mprof_ptr))->pcavailbase;
	(TREF(mprof_ptr))->pcavail = PROFCALLOC_DSBLKSIZE - SIZEOF(char *);
	CLEAR_PROFILING_TABLE();
	return;
}

/* Called in the beginning of any FOR loop iteration. */
void forchkhandler(char *return_address)
{
        trace_entry     	tmp_trc_tbl_entry;
	int			for_level_on_line;
	mprof_tree		*for_link, *for_node;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
        get_entryref_information(TRUE, &tmp_trc_tbl_entry);
	if (FALSE == (TREF(mprof_ptr))->is_tracing_ini)
	{
		(TREF(mprof_ptr))->is_tracing_ini = TRUE;
		(TREF(mprof_ptr))->curr_tblnd = (TREF(mprof_ptr))->head_tblnd = (mprof_tree *)new_node(&tmp_trc_tbl_entry);
		(TREF(mprof_ptr))->curr_tblnd->e.count = 1;
		for_node = (mprof_tree *)new_for_node(&tmp_trc_tbl_entry, return_address);
		for_node->e.count = 1;
		for_node->e.loop_level = 1;
		(TREF(mprof_ptr))->curr_tblnd->loop_link = (mprof_tree *)for_node;
	} else
	{
		(TREF(mprof_ptr))->curr_tblnd =
			(mprof_tree *)mprof_tree_insert(&((TREF(mprof_ptr))->head_tblnd), &tmp_trc_tbl_entry);
		if (NULL != (TREF(mprof_ptr))->curr_tblnd->loop_link)
		{	/* some FORs have been already been recorded for this line */
			for_link = (mprof_tree *)(TREF(mprof_ptr))->curr_tblnd->loop_link;
			for_level_on_line = 1;
			while (TRUE)
			{	/* same FOR, so just update the count */
				if (for_link->e.raddr == return_address)
				{
					for_link->e.count++;
					break;
				}
				/* new FOR for this line */
				if (NULL == for_link->loop_link)
				{
					for_node = (mprof_tree *)new_for_node(&tmp_trc_tbl_entry, return_address);
					for_node->e.count = 1;
					for_node->e.loop_level = for_level_on_line + 1;
					for_link->loop_link = (mprof_tree *)for_node;
					break;
				} else
				{
					for_link = (mprof_tree *)for_link->loop_link;
					for_level_on_line++;
				}
			}
		} else
		{	/* first FOR for this node */
			for_node = (mprof_tree *)new_for_node(&tmp_trc_tbl_entry, return_address);
			for_node->e.count = 1;
			for_node->e.loop_level = 1;
			(TREF(mprof_ptr))->curr_tblnd->loop_link = (mprof_tree *)for_node;
		}
	}
	return;
}

/* Called on each linestart and linefetch */
void pcurrpos(void)
{
	trace_entry	tmp_trc_tbl_entry;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(TREF(mprof_ptr));
	assert(TREF(prof_fp));
	TIMES(&(TREF(mprof_ptr))->tcurr); /* remember the new time */
	/* update the time of previous M line */
	if (NULL != (TREF(mprof_ptr))->curr_tblnd)
	{
		UPDATE_TIME((TREF(mprof_ptr))->curr_tblnd);
	}
	get_entryref_information(TRUE, &tmp_trc_tbl_entry);
	if (FALSE == (TREF(mprof_ptr))->is_tracing_ini)
	{
		(TREF(mprof_ptr))->is_tracing_ini = TRUE;
		(TREF(mprof_ptr))->curr_tblnd = (TREF(mprof_ptr))->head_tblnd = (mprof_tree *)new_node(&tmp_trc_tbl_entry);
		(TREF(mprof_ptr))->curr_tblnd->e.count = 1;
	} else
	{
		(TREF(mprof_ptr))->curr_tblnd =
			(mprof_tree *)mprof_tree_insert(&((TREF(mprof_ptr))->head_tblnd), &tmp_trc_tbl_entry);
		if (0 == (TREF(mprof_ptr))->curr_tblnd->e.count)
			(TREF(mprof_ptr))->curr_tblnd->e.count = 1;
		else
			(TREF(mprof_ptr))->curr_tblnd->e.count++;
	}
	(TREF(mprof_ptr))->tprev = (TREF(mprof_ptr))->tcurr;
	return;
}

char *pcalloc(unsigned int n)
{
	char **x;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	GTM64_ONLY(n = ((n + 7) & ~7);) 	/* same logic applied for alignment */
	NON_GTM64_ONLY(n = ((n + 3) & ~3);) 	/* make sure that it is quad-word aliged */
	if (n > (TREF(mprof_ptr))->pcavail)
	{
		if (*(TREF(mprof_ptr))->pcavailptr)
			(TREF(mprof_ptr))->pcavailptr = (char ** )*(TREF(mprof_ptr))->pcavailptr;
		else
		{
			x = (char **)malloc(PROFCALLOC_DSBLKSIZE);
			*(TREF(mprof_ptr))->pcavailptr = (char *)x;
			(TREF(mprof_ptr))->pcavailptr = x;
			*(TREF(mprof_ptr))->pcavailptr = 0;
		}
		(TREF(mprof_ptr))->pcavail = PROFCALLOC_DSBLKSIZE - SIZEOF(char *);
		memset((TREF(mprof_ptr))->pcavailptr + 1, 0, (TREF(mprof_ptr))->pcavail);
	}
	(TREF(mprof_ptr))->pcavail -= n;
	assert((TREF(mprof_ptr))->pcavail >= 0);
	return (char *)(TREF(mprof_ptr))->pcavailptr + (TREF(mprof_ptr))->pcavail + SIZEOF(char *);
}

/* Called upon entering a new label. The time account for a label excludes the time spent within any other label
 * called from it. An example:
 *
 *	d l1	|			t0
 *		|
 *		|
 *		| d l2	|		t1
 *			|
 *			|
 *			|
 *			| q		t2
 *		|
 *		|
 *		|
 *		|
 *		| q			t3
 *
 *	Results should be:
 *		l1 label:
 *			(t3 - t2) + (t1 - t0)
 *
 *		l2 label:
 *			(t2 - t1)
 */
void new_prof_frame(int real_frame)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(TREF(mprof_ptr));
	assert(TREF(prof_fp));
	/* a call to a routine, not IF or FOR with a DO */
	if (real_frame)
	{	/* update the time of the line in the parent frame */
		if (NULL != (TREF(mprof_ptr))->curr_tblnd)
		{
			TIMES(&(TREF(mprof_ptr))->tcurr);
			UPDATE_TIME((TREF(mprof_ptr))->curr_tblnd);
		}
		(TREF(prof_fp))->curr_node = (TREF(mprof_ptr))->curr_tblnd;
		(TREF(mprof_ptr))->curr_tblnd = NULL;
		/* create a new frame on the stack */
		TREF(prof_fp) = mprof_stack_push();
		(TREF(prof_fp))->rout_name = NULL;
		(TREF(prof_fp))->label_name = NULL;
		(TREF(prof_fp))->start = (TREF(mprof_ptr))->tprev = (TREF(mprof_ptr))->tcurr;
		(TREF(prof_fp))->carryover.tms_utime = 0;
		(TREF(prof_fp))->carryover.tms_stime = 0;
		(TREF(prof_fp))->dummy_stack_count = 0;
	} else
		(TREF(prof_fp))->dummy_stack_count++;
	return;
}

/* Called before leaving a label. */
void unw_prof_frame(void)
{
	trace_entry	e;
	trace_entry	tmp_trc_tbl_entry;
	mprof_tree	*t;
	struct tms	carryover;
	stack_frame	*save_fp;
	unsigned int    frame_usr_time, frame_sys_time;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(TREF(mprof_ptr));
	/* do not assert on prof_fp not being NULL, as it will be NULL in
	 * case we quit from a function without ever disabling tracing */
	if (NULL == TREF(prof_fp))
		return;
	/* we are leaving a real frame, not an IF or FOR with a DO */
	if (0 >= (TREF(prof_fp))->dummy_stack_count)
	{
		TIMES(&(TREF(mprof_ptr))->tcurr);
		/* update the time of last line in this frame before returning */
		if (NULL != (TREF(mprof_ptr))->curr_tblnd)
		{
			UPDATE_TIME((TREF(mprof_ptr))->curr_tblnd);
		}
		get_entryref_information(TRUE, &tmp_trc_tbl_entry);
		if ((TREF(prof_fp))->rout_name == &above_routine)
		{	/* it should have been filled in get_entryref_information */
			e.label_name = tmp_trc_tbl_entry.label_name;
			e.rout_name = tmp_trc_tbl_entry.rout_name;
		} else
		{
			e.label_name = (mident *)pcalloc(SIZEOF(mident));
			e.label_name->len = (TREF(prof_fp))->label_name->len;
			e.label_name->addr = pcalloc((unsigned int)e.label_name->len);
			memcpy(e.label_name->addr, (TREF(prof_fp))->label_name->addr, (TREF(prof_fp))->label_name->len);
			e.rout_name = (mident *)pcalloc(SIZEOF(mident));
			e.rout_name->len = (TREF(prof_fp))->rout_name->len;
			e.rout_name->addr = pcalloc((unsigned int)e.rout_name->len);
			memcpy(e.rout_name->addr, (TREF(prof_fp))->rout_name->addr, (TREF(prof_fp))->rout_name->len);
		}
		e.line_num = -1;
		/* insert/find a frame entry into/in the MPROF tree, -1 indicating that it is not a
		 * real line number, but rather an aggregation of several lines (comprising a label) */
		t = mprof_tree_insert(&((TREF(mprof_ptr))->head_tblnd), &e);
		/* update count and timing (from prof_fp) of frame I'm leaving */
		t->e.count++;
		frame_usr_time = ((TREF(mprof_ptr))->tcurr.tms_utime -
			(TREF(prof_fp))->start.tms_utime - (TREF(prof_fp))->carryover.tms_utime);
		frame_sys_time = ((TREF(mprof_ptr))->tcurr.tms_stime -
			(TREF(prof_fp))->start.tms_stime - (TREF(prof_fp))->carryover.tms_stime);
		t->e.usr_time += frame_usr_time;
		t->e.sys_time += frame_sys_time;
		/* not the first frame on the stack */
		if ((TREF(prof_fp))->prev)
		{
			carryover = (TREF(prof_fp))->carryover;
			/* move back up to parent frame */
			TREF(prof_fp) = mprof_stack_pop();
			(TREF(prof_fp))->carryover.tms_utime += (frame_usr_time + carryover.tms_utime);
			(TREF(prof_fp))->carryover.tms_stime += (frame_sys_time + carryover.tms_stime);
			/* restore the context of the parent frame */
			(TREF(mprof_ptr))->tprev = (TREF(mprof_ptr))->tcurr;
			(TREF(mprof_ptr))->curr_tblnd = (TREF(prof_fp))->curr_node;
			(TREF(prof_fp))->curr_node = NULL;
		} else
		{	/* This should only be true only if the View command is not at the top-most stack level, in which case
			 * add profiling information for the current frame. To prevent stack underflow, add a new frame before
			 * unwinding from this frame. */
			mprof_stack_pop();
			(TREF(mprof_ptr))->tprev = (TREF(mprof_ptr))->tcurr;
			(TREF(mprof_ptr))->curr_tblnd = NULL;
			TREF(prof_fp) = mprof_stack_push();
			save_fp = frame_pointer;
#			ifdef GTM_TRIGGER
			if (frame_pointer->type & SFT_TRIGR)
			{	/* in a trigger base frame, old_frame_pointer is NULL */
				assert(NULL == frame_pointer->old_frame_pointer);
				/* have a trigger baseframe, pick up stack continuation frame_pointer stored by base_frame() */
				frame_pointer = *(stack_frame **)(frame_pointer + 1);
			} else
#			endif
			frame_pointer = frame_pointer->old_frame_pointer;
			assert(frame_pointer);
			get_entryref_information(FALSE, NULL);
			frame_pointer = save_fp;
			if (NULL == TREF(prof_fp))
				return;
			(TREF(prof_fp))->start = (TREF(mprof_ptr))->tcurr;
			(TREF(prof_fp))->carryover.tms_utime = 0;
			(TREF(prof_fp))->carryover.tms_stime = 0;
			/* tag it so that next time, it will pick up label/routine info from current loc */
			(TREF(prof_fp))->rout_name = (mident *)&above_routine;
			(TREF(prof_fp))->label_name = NULL;
			(TREF(prof_fp))->dummy_stack_count = 0;
		}
	} else
		(TREF(prof_fp))->dummy_stack_count--;
	return;
}

/* Writes the data into the global. */
void crt_gbl(mprof_tree *p, boolean_t is_for)
{
	char		*c_top, *c_ref, ch;
	int		count, arg_index, subsc_len, tmp_str_len;
	INTPTR_T	start_point;
	mval		data;
	char		dataval[96];		/* big enough for data value */
	unsigned char	subsval[12];		/* see i2asc + 1 for null char */
	unsigned char	asc_line_num[12];	/* to hold the ascii equivalent of the line_num */
	unsigned char	*tmpnum, *end;
	mval		*spt;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (0 == p->e.count)
		return;
	count = (int)(TREF(mprof_ptr))->gvargs.count;
	spt = &(TREF(mprof_ptr))->subsc[count];
	/* global name --> ^PREFIX(<OPTIONAL ARGUMENTS>, "rout-name", "label-name", "line-num", "forloop") */
	spt->mvtype = MV_STR;
	spt->str.len = p->e.rout_name->len;
	spt->str.addr = (char *)pcalloc((unsigned int)spt->str.len);
	memcpy(spt->str.addr, p->e.rout_name->addr, spt->str.len);
	(TREF(mprof_ptr))->gvargs.args[count++] = spt++;
	spt->mvtype = MV_STR;
	if (0 != p->e.label_name->len)
	{
		spt->str.len = p->e.label_name->len;
		spt->str.addr = (char *)pcalloc((unsigned int)spt->str.len);
		memcpy(spt->str.addr, p->e.label_name->addr, spt->str.len);
	} else
	{	/* place holder before first label */
		spt->str.len = SIZEOF(MPROF_NULL_LABEL) - 1;
		spt->str.addr = (char *)pcalloc((unsigned int)spt->str.len);
		memcpy(spt->str.addr, MPROF_NULL_LABEL, spt->str.len);
	}
	(TREF(mprof_ptr))->gvargs.args[count++] = spt++;
	spt->mvtype = MV_STR;
	if (0 <= p->e.line_num)
	{
		tmpnum = i2asc(asc_line_num, (unsigned int) p->e.line_num);
		spt->str.len = INTCAST(tmpnum - asc_line_num);
		spt->str.addr = (char *)pcalloc((unsigned int)spt->str.len);
		memcpy(spt->str.addr, asc_line_num, spt->str.len);
		(TREF(mprof_ptr))->gvargs.args[count] = spt;
		count++;
		spt++;
	}
	/* for FOR loops */
	if (is_for)
	{
		spt->mvtype = MV_STR;
		spt->str.len = SIZEOF(MPROF_FOR_LOOP) - 1;
		spt->str.addr = (char *)pcalloc(SIZEOF(MPROF_FOR_LOOP) - 1);
		memcpy(spt->str.addr, MPROF_FOR_LOOP, spt->str.len);
		(TREF(mprof_ptr))->gvargs.args[count++] = spt++;
		/* write for level into the subscript as well */
		spt->mvtype = MV_STR;
		tmpnum = i2asc(subsval, p->e.loop_level);
		spt->str.len = INTCAST(tmpnum - subsval);
		spt->str.addr = (char *)pcalloc((unsigned int)spt->str.len);
		memcpy(spt->str.addr, subsval, spt->str.len);
		(TREF(mprof_ptr))->gvargs.args[count++] = spt++;
	}
	(TREF(mprof_ptr))->gvargs.count = count;
	callg((INTPTR_T(*)(intszofptr_t count_arg, ...))op_gvname, (gparam_list *)&(TREF(mprof_ptr))->gvargs);
	(TREF(mprof_ptr))->gvargs.count = (TREF(mprof_ptr))->curr_num_subscripts;
	/* data --> "count:cpu-time in user mode:cpu-time in sys mode:cpu-time total" */
	start_point = (INTPTR_T)&dataval[0];
	/* get count */
	tmpnum = (unsigned char *)&dataval[0];
	end = i2asc(tmpnum, p->e.count);
	tmpnum += ((end - tmpnum) > 0) ? (end - tmpnum) : (tmpnum - end);
	/* for non-FOR stuff get CPU time as well */
	if (!is_for)
	{
		*tmpnum = ':';
		tmpnum++;
		end = i2asc(tmpnum, p->e.usr_time);
		tmpnum += ((end - tmpnum) > 0) ? (end - tmpnum) : (tmpnum - end);
#		ifndef VMS
		*tmpnum = ':';
		tmpnum++;
		end = i2asc(tmpnum, p->e.sys_time);
		tmpnum += ((end - tmpnum) > 0) ? (end - tmpnum) : (tmpnum - end);
		*tmpnum = ':';
		tmpnum++;
		end = i2asc(tmpnum, p->e.sys_time + p->e.usr_time);
		tmpnum += ((end - tmpnum) > 0) ? (end - tmpnum) : (tmpnum - end);
#		endif
	}
	data.mvtype = MV_STR;
	data.str.len = (((INTPTR_T)tmpnum - start_point) > 0)
		? ((int)((INTPTR_T)tmpnum - start_point)) : ((int)(start_point - (INTPTR_T)tmpnum));
	data.str.addr = (char *)pcalloc((unsigned int)data.str.len);
	memcpy(data.str.addr, dataval, data.str.len);
	op_gvput(&data);
	return;
}

/* Fills the information about the current location in the code into the passed variable. */
STATICFNDEF void get_entryref_information(boolean_t line, trace_entry *tmp_trc_tbl_entry)
{
	boolean_t	line_reset;
	lab_tabent	*max_label, *label_table, *last_label;
	rhdtyp		*routine;
	stack_frame	*fp;
	int		status;
	unsigned char	*addr, *out_addr;
	unsigned char	temp[OFFSET_LEN];
	int4		*line_table, *last_line, len, ct;
	int4		offset, in_addr_offset;
	unsigned long	user_time, system_time;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	line_reset = FALSE;
	for (fp = frame_pointer; fp; fp = fp->old_frame_pointer)
	{
#		ifdef GTM_TRIGGER
		if (fp->type & SFT_TRIGR)
		{
			assert(NULL == fp->old_frame_pointer);
			/* have a trigger baseframe, pick up stack continuation frame_pointer stored by base_frame() */
			fp = *(stack_frame **)(fp + 1);
		}
#		endif
		assert(fp);
		if (ADDR_IN_CODE(fp->mpc, fp->rvector))
		{
			if (line_reset)
				addr = fp->mpc + 1;
			else
				addr = fp->mpc;
			break;
		} else
		{
			if (fp->type & SFT_ZTRAP || fp->type & SFT_DEV_ACT)
				line_reset = TRUE;
		}
	}
	if (NULL == fp)
	{
		TREF(prof_fp) = NULL;
		return;
	}
	routine = fp->rvector;
	routine = CURRENT_RHEAD_ADR(routine);
	label_table = LABTAB_ADR(routine);
	last_label = label_table + routine->labtab_len;
	max_label = label_table++;
	while (label_table < last_label)
	{
		if (addr > LABEL_ADDR(routine, label_table))
		{
			if (max_label->LABENT_LNR_OFFSET <= label_table->LABENT_LNR_OFFSET)
				max_label = label_table;
		}
		label_table++;
	}
	if (line)
	{
		tmp_trc_tbl_entry->rout_name = &routine->routine_name;
		tmp_trc_tbl_entry->label_name = &max_label->lab_name;
	}
	if (NULL == (TREF(prof_fp))->rout_name)
		(TREF(prof_fp))->rout_name = &routine->routine_name;
	if (NULL == (TREF(prof_fp))->label_name)
		(TREF(prof_fp))->label_name = &max_label->lab_name;
	if (!line)
		return;
	line_table = LABENT_LNR_ENTRY(routine, max_label);
	offset = 0;
	in_addr_offset = (int4)CODE_OFFSET(routine, addr);
	last_line = LNRTAB_ADR(routine);
	last_line += routine->lnrtab_len;
	for( ; ++line_table < last_line ; offset++)
	{
		if (in_addr_offset <= *line_table)
			break;
	}
	tmp_trc_tbl_entry->line_num = offset;
	return;
}

/* Parses the global variable name that the information will be dumped into, to make sure it is a valid gvn */
STATICFNDEF void parse_gvn(mval *gvn)
{
	boolean_t 		dot_seen;
	mval			*spt;
	char			*c_top, *c_ref, ch;
	unsigned int		count = 0;
	static mstr		mprof_mstr;	/* area to hold global and subscripts */
	char			*mpsp;		/* pointer into mprof_mstr area */
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	c_ref = gvn->str.addr;
	c_top = c_ref + gvn->str.len;
	if (!gvn->str.len || '^' != *c_ref++)
		rts_error(VARLSTCNT(4) ERR_NOTGBL, 2, gvn->str.len, gvn->str.addr);
	if (mprof_mstr.len < 4 * gvn->str.len)
	{	/* We are going to return an array of mvals pointing to global-name and subscript. We should
		 * never be needing more than 4 * gvn->str.len since the only expandable entity that can be
		 * passed is $j which uses up at least 3 characters (including '_' or ',') and expands to a
		 * maximum of 10 characters (see djbuff in getjobname.c). */
		if (mprof_mstr.len)
		{
			assert(mprof_mstr.addr);
			free(mprof_mstr.addr);
		}
		mprof_mstr.len = 4 * gvn->str.len;
		mprof_mstr.addr = (char *)malloc(mprof_mstr.len);
	}
	mpsp = mprof_mstr.addr;
	/* parse the global variable passed to insert the information */
	spt = &(TREF(mprof_ptr))->subsc[0];
	spt->mvtype = MV_STR;
	spt->str.addr = mpsp;
	ch = *mpsp++ = *c_ref++;
	if (!ISALPHA_ASCII(ch) && (ch != '%'))
		RTS_ERROR_VIEWNOTFOUND("Invalid global name");
	for ( ; (c_ref < c_top) && ('(' != *c_ref); )
	{
		ch = *mpsp++ = *c_ref++;
		if (!ISALNUM_ASCII(ch))
			RTS_ERROR_VIEWNOTFOUND("Invalid global name");
	}
	spt->str.len = INTCAST(mpsp - spt->str.addr);
	(TREF(mprof_ptr))->gvargs.args[count++] = spt++;
	spt->str.addr = (char *)mpsp;
	/* process subscripts, if any */
	if (c_ref++ < c_top)
	{
		for ( ; c_ref < c_top; )
		{
			spt->mvtype = MV_STR;
			ch = *c_ref;
			if ('\"' == ch)
			{
				c_ref++;
				for ( ; ; )
				{
					if (c_ref == c_top)
						RTS_ERROR_VIEWNOTFOUND("Right parenthesis expected");
					if ('\"' == *c_ref)
					{
						if (++c_ref == c_top)
							RTS_ERROR_VIEWNOTFOUND("Right parenthesis expected");
						if (*c_ref != '\"')
							break;
					}
					*mpsp++ = *c_ref++;
				}
			} else if ('$' == ch)
			{
				if (++c_ref == c_top)
					RTS_ERROR_VIEWNOTFOUND("Intrinsic value is incomplete");
				if (*c_ref != 'J' && *c_ref != 'j')
					RTS_ERROR_VIEWNOTFOUND("Intrinsic value passed is not $j");
				c_ref++; 	/* past 'J' */
				if ((c_ref < c_top) && (ISALPHA_ASCII(*c_ref)))
				{
					ch = *c_ref;
					if (c_top >= c_ref + 2)
					{
						if ((('O' == ch  || 'o' == ch) && ('B' == *(c_ref + 1) || 'b' == *(c_ref + 1))))
							c_ref += 2;
						else
							RTS_ERROR_VIEWNOTFOUND("Intrinsic value passed is not $j");
					} else
						RTS_ERROR_VIEWNOTFOUND("Intrinsic value is incomplete");
				}
				assert(10 > dollar_job.str.len);	/* to take care of 4 * gvn->str.len allocation above */
				memcpy(mpsp, dollar_job.str.addr, dollar_job.str.len);
				mpsp += dollar_job.str.len;
			} else
			{
				dot_seen = FALSE;
				if (!ISDIGIT_ASCII(ch)  &&  ch != '.'  &&  ch != '-'  &&  ch != '+')
					RTS_ERROR_VIEWNOTFOUND("Improperly formatted numeric subscript");
				if ('.' == ch)
					dot_seen = TRUE;
				*mpsp++ = *c_ref++;
				for ( ; ; )
				{
					if (c_ref == c_top)
						RTS_ERROR_VIEWNOTFOUND("Right parenthesis expected");
					if (!ISDIGIT_ASCII(*c_ref))
					{
						if (*c_ref != '.')
							break;
						else if (!dot_seen)
							dot_seen = TRUE;
						else
							RTS_ERROR_VIEWNOTFOUND("Improperly formatted numeric subscript");
					}
					*mpsp++ = *c_ref++;
				}
			}
			if (c_ref == c_top)
				RTS_ERROR_VIEWNOTFOUND("Right parenthesis expected");
			if ('_' == *c_ref)
			{
				c_ref++;
				continue;
			}
			spt->str.len = INTCAST(mpsp - spt->str.addr);
			if (MAX_GVSUBSCRIPTS <= count)
				rts_error(VARLSTCNT(1) ERR_MAXNRSUBSCRIPTS);
			(TREF(mprof_ptr))->gvargs.args[count++] = spt++;
			if (*c_ref != ',')
				break;
			spt->str.addr = mpsp;
			c_ref++;
		}
		assert(c_ref <= c_top);
		if (c_ref >= c_top)
			RTS_ERROR_VIEWNOTFOUND("Right parenthesis expected");
		if (*c_ref != ')')
			RTS_ERROR_VIEWNOTFOUND("Right parenthesis expected");
		if (++c_ref < c_top)
			RTS_ERROR_VIEWNOTFOUND("There are trailing characters after the global name");
	}
	assert((char *)mpsp <= mprof_mstr.addr + mprof_mstr.len);	/* ensure we haven't overrun the malloced buffer */
	(TREF(mprof_ptr))->gvargs.count = count;
	(TREF(mprof_ptr))->curr_num_subscripts = (int)(TREF(mprof_ptr))->gvargs.count;
	return;
}

void stack_leak_check(void)
{
	int	var_on_cstack;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	ifdef UNIX
	if (GTMTRIG_ONLY((0 < gtm_trigger_depth) ||) (0 < TREF(gtmci_nested_level)))
		return;
#	endif
	if (NULL == var_on_cstack_ptr)
		var_on_cstack_ptr = &var_on_cstack;
	if ((&var_on_cstack != var_on_cstack_ptr)
#	    ifdef	__i386	/* for 32-bit Linux allow a two pointer variation to accommodate ZHELP */
	    && ((SIZEOF(var_on_cstack) * 2) < ABS(&var_on_cstack - var_on_cstack_ptr))
#	    endif
	    )
		GTMASSERT;
	return;
}
