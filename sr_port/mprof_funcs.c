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
GBLREF 	unsigned char 		*profstack_base, *profstack_top, *prof_msp, *profstack_warn;
GBLREF 	unsigned char 		*prof_stackptr;
GBLREF 	stack_frame_prof 	*prof_fp;
GBLREF 	mval			dollar_job;
GBLREF	uint4			process_id;
GBLREF	int * volatile		var_on_cstack_ptr;	/* volatile so that nothing gets optimized out */

LITDEF  MIDENT_CONST(above_routine, "*above*");
LITDEF  MIDENT_CONST(overflow_routine, "*overflow*");

#define	MAX_MPROF_STACK_LEVEL	1024

struct mprof_struct
{
	struct tms		tprev, tcurr;
	mprof_tree		*currnd_stk[MAX_MPROF_STACK_LEVEL];
	mprof_tree		*head_tblnd, *curr_tblnd, *ovrflw;
	int			line_prof_stack;
	int			curr_num_subscripts;
	char			**pcavailptr, **pcavailbase;
	int			pcavail;
	boolean_t		is_tracing_ini;
	mval			subsc[MAX_GVSUBSCRIPTS];
	gvargs_t		gvargs;
	mval			gbl_to_fill;
	boolean_t		overflown;
	boolean_t		last_label_recorded;
};
static struct mprof_struct *mprof_ptr;

static struct mprof_tree *last_mprof_node;

#define OVERFLOW_STRING	":INCOMPLETE DATA: MAXTRACELEVEL"
#define MPROF_NULL_LABEL "^"
#define MPROF_FOR_LOOP	"FOR_LOOP"
#define UPDATE_TIME(x)	x->e.usr_time += (mprof_ptr->tcurr.tms_utime - mprof_ptr->tprev.tms_utime);\
			x->e.sys_time += (mprof_ptr->tcurr.tms_stime - mprof_ptr->tprev.tms_stime);
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
STATICFNDCL void pcfree(void);

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
}
#else
#error UNSUPPORTED PLATFORM
#endif

void turn_tracing_on(mval *gvn)
{
	struct tms	*curr;
	trace_entry	tmp_trc_tbl_entry;
	/* For some reason the construct below and/or the strcpy calls made GTMSHR writeable on Vax
	   which means it was no longer shareable. Doing this differently makes it all still work.
	   Tiz a kludge authored by Satya..
	   struct mprof_tree p = {{"*dummy**", "*dummy**", "*dummy*", 0, 0, 0}, 0 , 0 }; */

	if (is_tracing_on)
	{
		gtm_putmsg(VARLSTCNT(1) ERR_TRACINGON);
		return;
	}
	if (0 == gvn->str.len || '^' != gvn->str.addr[0])
		rts_error(VARLSTCNT(4) ERR_NOTGBL, 2, gvn->str.len, gvn->str.addr);
	if (!mprof_ptr)
	{
		mprof_ptr = (struct mprof_struct *)malloc(SIZEOF(struct mprof_struct));
		memset(mprof_ptr, 0, SIZEOF(struct mprof_struct));
	}
	parse_gvn(gvn);
	mprof_ptr->gbl_to_fill = *gvn;
	mprof_ptr->gbl_to_fill.str.addr = (char *)malloc(gvn->str.len); /*len was already setup*/
	memcpy(mprof_ptr->gbl_to_fill.str.addr, gvn->str.addr, gvn->str.len);
	if (!mprof_ptr->pcavailbase)
	{
		mprof_ptr->pcavailbase = (char **) malloc(PROFCALLOC_DSBLKSIZE);
		*mprof_ptr->pcavailbase = 0;
	}
	mprof_ptr->pcavailptr = mprof_ptr->pcavailbase;
	mprof_ptr->pcavail = PROFCALLOC_DSBLKSIZE - SIZEOF(char *);
	memset(mprof_ptr->pcavailptr + 1, 0, mprof_ptr->pcavail);
	curr = &(mprof_ptr->tprev);
	TIMES(curr);
	prof_stackptr = (unsigned char *)malloc(TOTAL_SIZE_OF_PROFILING_STACKS);
	prof_msp = profstack_base = prof_stackptr + (TOTAL_SIZE_OF_PROFILING_STACKS - GUARD_RING_FOR_PROFILING_STACK);
	profstack_top = prof_stackptr;
	profstack_warn = profstack_top + GUARD_RING_FOR_PROFILING_STACK;
	prof_fp = (stack_frame_prof *) (prof_msp -= SIZEOF(stack_frame_prof));
	memset(prof_fp, 0, SIZEOF(*prof_fp));
	get_entryref_information(FALSE, NULL);
	tmp_trc_tbl_entry.rout_name = NULL; /* initialize */
	tmp_trc_tbl_entry.label_name = NULL;
	mprof_ptr->curr_tblnd = mprof_ptr->head_tblnd = NULL;
	mprof_ptr->overflown = FALSE;
	last_mprof_node = (mprof_tree *)NULL;
	prof_fp->start.tms_stime = (*curr).tms_stime;
	prof_fp->start.tms_utime = (*curr).tms_utime;
	prof_fp->dummy_stack_count = 0;

	POPULATE_PROFILING_TABLE();
	is_tracing_on = TRUE;
}

STATICFNDEF void pcfree(void)
{
	mprof_ptr->pcavailptr = mprof_ptr->pcavailbase;
	mprof_ptr->pcavail = PROFCALLOC_DSBLKSIZE - SIZEOF(char *);
	return;
}

void turn_tracing_off (mval *gvn)
{
	if (FALSE == is_tracing_on)
		return;
	assert(mprof_ptr);
	TIMES(&mprof_ptr->tcurr);

	/* update the time of previous M line */
	if (mprof_ptr->overflown)
	{
		mprof_ptr->ovrflw->e.usr_time += (mprof_ptr->tcurr.tms_utime - mprof_ptr->ovrflw->e.usr_time);
		mprof_ptr->ovrflw->e.sys_time += (mprof_ptr->tcurr.tms_stime - mprof_ptr->ovrflw->e.sys_time);
	} else if (NULL != mprof_ptr->curr_tblnd)
	{
		UPDATE_TIME(mprof_ptr->curr_tblnd);
	}

	if (NULL != gvn)
		parse_gvn(gvn);
	is_tracing_on = mprof_ptr->is_tracing_ini = FALSE;
	assert(0 != mprof_ptr->gbl_to_fill.str.addr);
	free(mprof_ptr->gbl_to_fill.str.addr);
	mprof_ptr->gbl_to_fill.str.addr = NULL;
	mprof_tree_walk(mprof_ptr->head_tblnd);
	free(prof_stackptr);
	prof_stackptr = NULL;
	pcfree();
	CLEAR_PROFILING_TABLE();
}

void forchkhandler(char *return_address)
{
        trace_entry     	tmp_trc_tbl_entry;
	int			for_level_on_line;
	mprof_tree		*for_link, *for_node;

	if (mprof_ptr->overflown)
	{
		return;
	}

        get_entryref_information(TRUE, &tmp_trc_tbl_entry);
	if (FALSE == mprof_ptr->is_tracing_ini)
	{
		mprof_ptr->is_tracing_ini = TRUE;
		mprof_ptr->curr_tblnd = mprof_ptr->head_tblnd = (mprof_tree *)new_node(&tmp_trc_tbl_entry);
		mprof_ptr->curr_tblnd->e.count = 1;
		for_node = (mprof_tree *)new_for_node(&tmp_trc_tbl_entry, return_address);
		for_node->e.count = 1;
		for_node->e.loop_level = 1;
		mprof_ptr->curr_tblnd->loop_link = (mprof_tree *)for_node;
	} else
	{
		mprof_ptr->curr_tblnd = (mprof_tree *)mprof_tree_insert(&(mprof_ptr->head_tblnd), &tmp_trc_tbl_entry);

		if (NULL != mprof_ptr->curr_tblnd->loop_link)
		{	/* FORs have been saved for this line */
			for_link = (mprof_tree *)mprof_ptr->curr_tblnd->loop_link;
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
			mprof_ptr->curr_tblnd->loop_link = (mprof_tree *)for_node;
		}
	}
}

void pcurrpos(int inside_for_loop)
{
	trace_entry	tmp_trc_tbl_entry;

	assert(mprof_ptr);
	if (!mprof_ptr)
		return;	/* protect in pro build */

	if (mprof_ptr->overflown)
	{
		return;
	}

	TIMES(&mprof_ptr->tcurr); /* remember the new time */

	/* update the time of previous M line */
	if (NULL != mprof_ptr->curr_tblnd)
	{
		UPDATE_TIME(mprof_ptr->curr_tblnd);
	}

	get_entryref_information(TRUE, &tmp_trc_tbl_entry);

	if (FALSE == mprof_ptr->is_tracing_ini)
	{
		mprof_ptr->is_tracing_ini = TRUE;
		mprof_ptr->curr_tblnd = mprof_ptr->head_tblnd = (mprof_tree *)new_node(&tmp_trc_tbl_entry);
		mprof_ptr->curr_tblnd->e.count = 1;
	} else
	{
		mprof_ptr->curr_tblnd = (mprof_tree *)mprof_tree_insert(&(mprof_ptr->head_tblnd), &tmp_trc_tbl_entry);

		if (0 == mprof_ptr->curr_tblnd->e.count)
			mprof_ptr->curr_tblnd->e.count = 1;
		else
			mprof_ptr->curr_tblnd->e.count += 1;
	}

	mprof_ptr->tprev = mprof_ptr->tcurr;
}

char *pcalloc(unsigned int n)
{
	char **x;

	GTM64_ONLY(n = ((n + 7) & ~7);) /* same logic applied for alignment */
	NON_GTM64_ONLY(n = ((n + 3) & ~3);) /* make sure that it is quad-word aliged */
	if (n > mprof_ptr->pcavail)
	{
		if (*mprof_ptr->pcavailptr)
			mprof_ptr->pcavailptr = (char ** ) *mprof_ptr->pcavailptr;
		else
		{
			x = (char **) malloc(PROFCALLOC_DSBLKSIZE);
			*mprof_ptr->pcavailptr = (char *) x;
			mprof_ptr->pcavailptr = x;
			*mprof_ptr->pcavailptr = 0;
		}
		mprof_ptr->pcavail = PROFCALLOC_DSBLKSIZE - SIZEOF(char *);
		memset(mprof_ptr->pcavailptr + 1, 0, mprof_ptr->pcavail);
	}
	mprof_ptr->pcavail -= n;
	assert(mprof_ptr->pcavail >= 0);
	return (char *) mprof_ptr->pcavailptr + mprof_ptr->pcavail + SIZEOF(char *);
}

void new_prof_frame(int real_frame)
{
	/*****************************************************************************************
	The time for frames accounts for the time spent within that frame,
	i.e. does not include time spent within other frames called from this frame.
	As an example:
	d l1	|			t0
		|
		|
		| d l2	|		t1
			|
			|
			|
			| q		t2
		|
		|
		|
		|
		| q			t3

	Results should be:
		l1 frame:
		(t3-t2)+(t1-t0)

			l2 frame
			(t2-t1)

	Calculations:
	at t0: 	prof_fp(l1)	=t0 (in new_prof_frame)
	at t1:	prof_fp(l1)	=t1-t0  (in new_prof_frame)
		prof_fp(l2)	=t1
	at t2:	in unw_prof_frame:
		prof_fp(l2)	=t2-t1  and it is saved back into the tree
		prof_fp(l1)	=t2-(prev.value)=t2-(t1-t0)
	at t3:  in unw_prof_frame:
		prof_fp(l1)	=t3-(cur_value of prof_fp(l1))
				=t3-(t2-(t1-t0))=t3-t2+t1-t0
				=(t3-t2)+(t1-t0)
	******************************************************************************************/
	stack_frame_prof	*psf;
	mprof_tree		*ovrflw_node;
	trace_entry		e;

	/* a call to a routine, not IF or FOR with DO */
	if (real_frame)
	{
		/* exceeded MPROF stack level; will not record deeper frames */
		if (MAX_MPROF_STACK_LEVEL <= ++mprof_ptr->line_prof_stack)
		{	/* print warning message if stack was not overflown before */
			if (!mprof_ptr->overflown)
			{
				/* update the time of previous M line */
				if (NULL != mprof_ptr->curr_tblnd)
				{
					UPDATE_TIME(mprof_ptr->curr_tblnd);
				}
				gtm_putmsg(VARLSTCNT(3) ERR_MAXTRACELEVEL, 1, MAX_MPROF_STACK_LEVEL);
				mprof_ptr->overflown = TRUE;
				TIMES(&mprof_ptr->tcurr);
				e.label_name = (mident *)&overflow_routine;
				e.rout_name = (mident *)&overflow_routine;
				e.line_num = -2;
				mprof_ptr->ovrflw = mprof_tree_insert(&(mprof_ptr->head_tblnd), &e);
				/*update count and timing (from prof_fp) of frame I'm leaving*/
				mprof_ptr->ovrflw->e.count = 1;
				mprof_ptr->ovrflw->e.usr_time = mprof_ptr->tcurr.tms_utime;
				mprof_ptr->ovrflw->e.sys_time = mprof_ptr->tcurr.tms_stime;
			}
			return;
		} else
		{
			/* update the time of the line in the parent frame */
			if (NULL != mprof_ptr->curr_tblnd)
			{
				TIMES(&mprof_ptr->tcurr);
				UPDATE_TIME(mprof_ptr->curr_tblnd);
			}
			mprof_ptr->currnd_stk[mprof_ptr->line_prof_stack] = mprof_ptr->curr_tblnd;
		}
		mprof_ptr->curr_tblnd = NULL;
		/* create a new frame on the stack */
		psf = (stack_frame_prof *) (prof_msp -= SIZEOF(stack_frame_prof));
		psf->prev = prof_fp;
		psf->rout_name = NULL;
		psf->label_name = NULL;

		psf->start = mprof_ptr->tprev = mprof_ptr->tcurr;
		prof_fp = psf;
		prof_fp->dummy_stack_count = 0;
	} else
		prof_fp->dummy_stack_count += 1;
	return;
}

void unw_prof_frame(void)
{
	trace_entry	e;
	trace_entry	tmp_trc_tbl_entry;
	mprof_tree	*t;
	struct tms	curr;
	stack_frame	*save_fp;
	unsigned int    frame_usr_time, frame_sys_time;

	assert(mprof_ptr);
	if (!mprof_ptr)
		return;	/* protect in pro build */

	if (NULL == prof_fp)
		return;
	/* we are leaving a real frame, not an IF or FOR with DO */
	if (0 >= prof_fp->dummy_stack_count)
	{
		/* if we have previously exceeded the maximum MPROF stack depth,
		 * we had better be aware of it; hence the assert */
		if (mprof_ptr->overflown)
		{
			if (mprof_ptr->last_label_recorded)
			{
				mprof_ptr->line_prof_stack--;
				return;
			}
			mprof_ptr->last_label_recorded = TRUE;
		}
		TIMES(&mprof_ptr->tcurr);
		/* update the time of last line in this frame before returning */
		if (NULL != mprof_ptr->curr_tblnd)
		{
			UPDATE_TIME(mprof_ptr->curr_tblnd);
		}

		get_entryref_information(TRUE, &tmp_trc_tbl_entry);
		if (NULL == prof_fp)
			return;
		if (prof_fp->rout_name == &above_routine)
		{
			/* it should have been filled in get_entryref_information */
			e.label_name = tmp_trc_tbl_entry.label_name;
			e.rout_name = tmp_trc_tbl_entry.rout_name;
		} else
		{
			e.label_name = (mident *) pcalloc(SIZEOF(mident));
			e.label_name->len = prof_fp->label_name->len;
			e.label_name->addr = pcalloc((unsigned int)e.label_name->len);
			memcpy(e.label_name->addr, prof_fp->label_name->addr, prof_fp->label_name->len);
			e.rout_name = (mident *) pcalloc(SIZEOF(mident));
			e.rout_name->len =  prof_fp->rout_name->len;
			e.rout_name->addr = pcalloc((unsigned int)e.rout_name->len);
			memcpy(e.rout_name->addr, prof_fp->rout_name->addr, prof_fp->rout_name->len);
		}
		e.line_num = -1;

		/* insert/find a frame entry into/in the MPROF tree, -1 indicating that it is
		 * not a real line number, but rather an aggregation of several lines */
		t = mprof_tree_insert(&(mprof_ptr->head_tblnd), &e);
		/*update count and timing (from prof_fp) of frame I'm leaving*/
		t->e.count++;
		frame_usr_time = (mprof_ptr->tcurr.tms_utime - prof_fp->start.tms_utime);
		frame_sys_time = (mprof_ptr->tcurr.tms_stime - prof_fp->start.tms_stime);
		t->e.usr_time += frame_usr_time;
		t->e.sys_time += frame_sys_time;
		/* not the first frame in the stack */
		if (prof_fp->prev)
		{
			if (0 < mprof_ptr->line_prof_stack)
			{	/* restore the context of the parent frame */
				mprof_ptr->tprev = mprof_ptr->tcurr;
				mprof_ptr->curr_tblnd = mprof_ptr->currnd_stk[mprof_ptr->line_prof_stack];
				mprof_ptr->line_prof_stack--;
			} else
				GTMASSERT;
			/* move back up to parent frame */
			prof_msp = (unsigned char *)prof_fp + SIZEOF(stack_frame_prof);
			prof_fp = prof_fp->prev;
			prof_fp->start.tms_utime += frame_usr_time;
			prof_fp->start.tms_stime += frame_sys_time;
		} else
		{
			/* This should only be true only if the View command is not at
			 * the top-most stack level. In which case add profiling information
			 * for the quit statement. */
			mprof_ptr->tprev = mprof_ptr->tcurr;
			mprof_ptr->curr_tblnd = NULL;
			prof_fp = (stack_frame_prof *)prof_msp;
			save_fp = frame_pointer;
#			ifdef GTM_TRIGGER
			if (frame_pointer->type & SFT_TRIGR)
			{	/* In a trigger base frame, old_frame_pointer is NULL */
				assert(NULL == frame_pointer->old_frame_pointer);
				/* Have a trigger baseframe, pick up stack continuation frame_pointer stored by base_frame() */
				frame_pointer = *(stack_frame **)(frame_pointer + 1);
			} else
#			endif
				frame_pointer = frame_pointer->old_frame_pointer;
			assert(frame_pointer);
			get_entryref_information(FALSE, NULL);
			frame_pointer = save_fp;
			if (NULL == prof_fp)
				return;
			prof_fp->prev = (stack_frame_prof *)NULL;
			prof_fp->start = mprof_ptr->tcurr;
			prof_fp->prev = NULL;
			/*tag it so that next time, it will pick up label/routine info from current loc*/
			prof_fp->rout_name = (mident *)&above_routine;
			prof_fp->label_name = NULL;
			prof_fp->dummy_stack_count = 0;
		}
	} else
		prof_fp->dummy_stack_count--;
	return;
}

void crt_gbl(mprof_tree *p, boolean_t is_for)
{
	/* Write the data into the global
	 */
	char		*c_top, *c_ref, ch;
	int		count, arg_index, subsc_len, tmp_str_len;
	INTPTR_T	start_point;
	mval		data;
	char		dataval[96];	/* big enough for data value */
	unsigned char	subsval[12];	/* see i2asc + 1 for null char */
	unsigned char	asc_line_num[12];	/* to hold the ascii equivalent of the line_num */
	unsigned char	*tmpnum, *end;
	mval		*spt;

	if (0 == p->e.count)
		return;
	count = (int)mprof_ptr->gvargs.count;
	spt = &mprof_ptr->subsc[count];
	/* Global name --> ^PREFIX(<OPTIONAL ARGUMENTS>, "rout-name", "label-name", "line-num", "forloop") */
	spt->mvtype = MV_STR;
	spt->str.len = p->e.rout_name->len;
	spt->str.addr = (char *)pcalloc((unsigned int)spt->str.len);
	memcpy(spt->str.addr, p->e.rout_name->addr, spt->str.len);
	mprof_ptr->gvargs.args[count++] = spt++;
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
	mprof_ptr->gvargs.args[count++] = spt++;
	spt->mvtype = MV_STR;
	if (0 <= p->e.line_num)
	{
		tmpnum = i2asc(asc_line_num, (unsigned int) p->e.line_num);
		spt->str.len = INTCAST(tmpnum - asc_line_num);
		spt->str.addr = (char *)pcalloc((unsigned int)spt->str.len);
		memcpy(spt->str.addr, asc_line_num, spt->str.len);
		mprof_ptr->gvargs.args[count] = spt;
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
		mprof_ptr->gvargs.args[count++] = spt++;
		/*write for level into the subscript as well*/
		spt->mvtype = MV_STR;
		tmpnum = i2asc(subsval, p->e.loop_level);
		spt->str.len = INTCAST(tmpnum - subsval);
		spt->str.addr = (char *)pcalloc((unsigned int)spt->str.len);
		memcpy(spt->str.addr, subsval, spt->str.len);
		mprof_ptr->gvargs.args[count++] = spt++;
	}
	mprof_ptr->gvargs.count = count;
	callg((INTPTR_T(*)(intszofptr_t count_arg, ...))op_gvname, (gparam_list *)&mprof_ptr->gvargs);
	mprof_ptr->gvargs.count = mprof_ptr->curr_num_subscripts;
	/* Data --> "count:cpu-time in user mode:cpu-time in sys mode:cpu-time total" */
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
	if ((mprof_ptr->overflown) && (-1 == p->e.line_num))
	{
		tmp_str_len = data.str.len;
		data.str.len += SIZEOF(OVERFLOW_STRING) - 1;
		data.str.addr = (char *)pcalloc((unsigned int)data.str.len);
		MEMCPY_LIT(dataval + tmp_str_len, OVERFLOW_STRING);

	} else
		data.str.addr = (char *)pcalloc((unsigned int)data.str.len);
	memcpy(data.str.addr, dataval, data.str.len);
	op_gvput(&data);
	return;
}

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

	line_reset = FALSE;
	for (fp = frame_pointer; fp; fp = fp->old_frame_pointer)
	{
#		ifdef GTM_TRIGGER
		if (fp->type & SFT_TRIGR)
		{
			assert(NULL == fp->old_frame_pointer);
			/* Have a trigger baseframe, pick up stack continuation frame_pointer stored by base_frame() */
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
		prof_fp = NULL;
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
	if (NULL == prof_fp->rout_name)
		prof_fp->rout_name = &routine->routine_name;
	if (NULL == prof_fp->label_name)
		prof_fp->label_name = &max_label->lab_name;
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
}

STATICFNDEF void parse_gvn(mval *gvn)
{
	/* parse the global variable name the information will be dumped into
	 * to make sure it is a valid gvn
	 */
	boolean_t 		dot_seen;
	mval			*spt;
	char			*c_top, *c_ref, ch;
	unsigned int		count = 0;
	static mstr		mprof_mstr;	/* area to hold global and subscripts */
	char			*mpsp;		/* pointer into mprof_mstr area */

	c_ref = gvn->str.addr;
	c_top = c_ref + gvn->str.len;
	if (!gvn->str.len || '^' != *c_ref++)
		rts_error(VARLSTCNT(4) ERR_NOTGBL, 2, gvn->str.len, gvn->str.addr);
	if (mprof_mstr.len < 4 * gvn->str.len)
	{	/* We are going to return an array of mvals pointing to global-name and subscripts.
		 * We should never be needing more than 4 * gvn->str.len since the only expandable entity
		 * 	that can be passed is $j which uses up at least 3 characters (including '_' or ',')
		 *	and expands to a maximum of 10 characters (see djbuff in getjobname.c).
		 */
		if (mprof_mstr.len)
		{
			assert(mprof_mstr.addr);
			free(mprof_mstr.addr);
		}
		mprof_mstr.len = 4 * gvn->str.len;
		mprof_mstr.addr = (char *)malloc(mprof_mstr.len);
	}
	mpsp = mprof_mstr.addr;
	/* Parse the global variable passed to insert the information */
	spt = &mprof_ptr->subsc[0];
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
	mprof_ptr->gvargs.args[count++] = spt++;
	spt->str.addr = (char *)mpsp;
	/* Process subscripts if any */
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
				c_ref++; 	/* Past 'J' */
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
			mprof_ptr->gvargs.args[count++] = spt++;
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
	assert((char *)mpsp <= mprof_mstr.addr + mprof_mstr.len);	/* Ensure we haven't overrun the malloced buffer */
	mprof_ptr->gvargs.count = count;
	mprof_ptr->curr_num_subscripts = (int)mprof_ptr->gvargs.count;
}

void stack_leak_check(void)
{
	int	var_on_cstack;

	if (NULL == var_on_cstack_ptr)
		var_on_cstack_ptr = &var_on_cstack;
	if ((&var_on_cstack != var_on_cstack_ptr)
#	    ifdef	__i386	/* for 32-bit Linux allow a two pointer variation to accommodate ZHELP */
	    && ((SIZEOF(var_on_cstack) * 2) < ABS(&var_on_cstack - var_on_cstack_ptr))
#	    endif
	    )
		GTMASSERT;
}
