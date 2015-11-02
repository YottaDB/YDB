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

#if defined(UNIX)
#include "gtm_stdio.h"
#include <errno.h>
#include "gtm_fcntl.h"
#include <signal.h>
#include "gtm_stat.h"
#include "gtm_times.h"
#include "gtm_unistd.h"
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
/* #include "xfer_enum.h" */
#include "op.h"
#include "hashtab_mname.h"	/* needed for lv_val.h */
#include "lv_val.h"
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

static const MIDENT_CONST(above_routine, "*above*");

#define	MAX_MPROF_STACK_LEVEL	1024
#ifdef 	MPROF_DEBUGGING
#define	MPROF_DEBUGGING 1
#include "util.h"
#define PRINT_PROF_TREE(x)	{ util_out_print(x, FALSE); mprof_tree_print(mprof_ptr->head_tblnd, 0, 1);}
#define PRINT_PROF_TREE_ELEM(x)	{ util_out_print(x, FALSE); mprof_tree_print(mprof_ptr->curr_tblnd, 0, -1);}
#else
#define PRINT_PROF_TREE(x)
#define PRINT_PROF_TREE_ELEM(x)
#endif

struct mprof_struct
{
	 struct tms		tprev, tcurr;
	 struct tms		time_stack[MAX_MPROF_STACK_LEVEL];
	 mprof_tree		*head_tblnd, *curr_tblnd;
	 mprof_tree		*currnd_stk[MAX_MPROF_STACK_LEVEL];
	 unsigned int		loop_info_stk[MAX_MPROF_STACK_LEVEL]; /*for DO's*/
	 unsigned int		for_level;
	 int			inside_for_loop_state;
	 int			line_prof_stack;
	 int			curr_num_subscripts;
	 char			**pcavailptr, **pcavailbase;
	 int			pcavail;
	 boolean_t		is_tracing_ini;
	 mval			subsc[MAX_GVSUBSCRIPTS];
	 gvargs_t		gvargs;
	 mval			gbl_to_fill;
	 int			overflowed_levels;
};
static struct mprof_struct *mprof_ptr;

#define OVERFLOW_STRING	":INCOMPLETE DATA: MAXTRACELEVEL"
#define MPROF_NULL_LABEL "^"
#define MPROF_FOR_LOOP	"FOR_LOOP"
#define UPDATE_TIME(x)	x->e.usr_time += (mprof_ptr->tcurr.tms_utime - mprof_ptr->tprev.tms_utime);\
			x->e.sys_time += (mprof_ptr->tcurr.tms_stime - mprof_ptr->tprev.tms_stime);
#define MPROF_INCR_COUNT		mprof_ptr->curr_tblnd->e.count += 1
#define LINK_NEW(x) 	if (NULL == x->loop_link) \
				x->loop_link = (mprof_tree *)new_node(&x->e)
#define SAME_LINE_FUNC(rout1, rout2, lab1, lab2, line1, line2)				\
			(((line1) == (line2)) && ((lab1) == (lab2) || MIDENT_EQ((lab1), (lab2))) && \
			((rout1) == (rout2) || MIDENT_EQ((rout1), (rout2))))

#define SAME_LINE	SAME_LINE_FUNC(tmp_trc_tbl_entry.rout_name,  mprof_ptr->curr_tblnd->e.rout_name, \
				       tmp_trc_tbl_entry.label_name, mprof_ptr->curr_tblnd->e.label_name, \
				       tmp_trc_tbl_entry.line_num,   mprof_ptr->curr_tblnd->e.line_num)
#define RTS_ERROR_VIEWNOTFOUND(x)	rts_error(VARLSTCNT(8) ERR_VIEWNOTFOUND, 2, gvn->str.len, gvn->str.addr, \
						ERR_TEXT, 2, RTS_ERROR_STRING(x));

#ifdef UNIX
#define TIMES			times
#elif defined(VMS)
#define TIMES			get_cputime
#endif

#if defined(VMS)
static void get_cputime (struct tms *curr)
{
	int4	cpu_time_used;
	int	status;
	int	jpi_code = JPI$_CPUTIM;
	error_def(ERR_SYSCALL);

	 if ((status = lib$getjpi(&jpi_code, &process_id, 0, &cpu_time_used, 0, 0)) != SS$_NORMAL)
		rts_error(VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("LIB$GETJPI"), CALLFROM, status);
	curr->tms_utime = cpu_time_used;
	curr->tms_stime = 0;
}
#endif

static void parse_gvn(mval *);
static void get_entryref_information(boolean_t, trace_entry *);

void turn_tracing_on(mval *gvn)
{
	struct tms	curr;
	trace_entry	tmp_trc_tbl_entry;
	/* For some reason the construct below and/or the strcpy calls made GTMSHR writeable on Vax
	   which means it was no longer shareable. Doing this differently makes it all still work.
	   Tiz a kludge authored by Satya..
	   struct mprof_tree p = {{"*dummy**", "*dummy**", "*dummy*", 0, 0, 0}, 0 , 0 }; */

	error_def(ERR_NOTGBL);
	error_def(ERR_TRACINGON);
	if (is_tracing_on)
	{
		gtm_putmsg(VARLSTCNT(1) ERR_TRACINGON);
		return;
	}
	if (0 == gvn->str.len || '^' != gvn->str.addr[0])
		rts_error(VARLSTCNT(4) ERR_NOTGBL, 2, gvn->str.len, gvn->str.addr);
	if (!mprof_ptr)
	{
		mprof_ptr = (struct mprof_struct *)malloc(sizeof(struct mprof_struct));
		memset(mprof_ptr, 0, sizeof(struct mprof_struct));
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
	mprof_ptr->pcavail = PROFCALLOC_DSBLKSIZE - sizeof(char *);
	memset(mprof_ptr->pcavailptr + 1, 0, mprof_ptr->pcavail);
	TIMES(&curr);
	prof_stackptr = (unsigned char *)malloc(TOTAL_SIZE_OF_PROFILING_STACKS);
	prof_msp = profstack_base = prof_stackptr + (TOTAL_SIZE_OF_PROFILING_STACKS - GUARD_RING_FOR_PROFILING_STACK);
	profstack_top = prof_stackptr;
	profstack_warn = profstack_top + GUARD_RING_FOR_PROFILING_STACK;
	prof_fp = (stack_frame_prof *) (prof_msp -= sizeof(stack_frame_prof));
	memset(prof_fp, 0, sizeof(*prof_fp));
	get_entryref_information(FALSE, NULL);
	tmp_trc_tbl_entry.rout_name = NULL; /* initialize */
	tmp_trc_tbl_entry.label_name = NULL;
	mprof_ptr->curr_tblnd = mprof_ptr->head_tblnd = (mprof_tree *)new_node(&tmp_trc_tbl_entry);
	prof_fp->sys_time = curr.tms_stime;
	prof_fp->usr_time = curr.tms_utime;
	prof_fp->dummy_stack_count = 0;
	POPULATE_PROFILING_TABLE();
	is_tracing_on = TRUE;
	mprof_ptr->inside_for_loop_state = 0;
	mprof_ptr->for_level = 0;
}

static void pcfree(void)
{
	mprof_ptr->pcavailptr = mprof_ptr->pcavailbase;
	mprof_ptr->pcavail = PROFCALLOC_DSBLKSIZE - sizeof(char *);
	return;
}

void turn_tracing_off (mval *gvn)
{
	if (FALSE == is_tracing_on)
		return;
	PRINT_PROF_TREE("");
	assert(mprof_ptr);
	TIMES(&mprof_ptr->tcurr);
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

void pcurrpos(int inside_for_loop)
{
	/* This function actually counts the line (number of executions and timing info)
	 * It recognizes 4 "states" (inside_for_loop)
	 * MPROF_INTOFOR	: going into loop
	 * MPROF_OUTOFFOR	: coming out of a loop
	 * MPROF_LINEFETCH 	: from op_mproflinefetch
	 * MPROF_LINESTART	: from op_mproflinestart
	 *
	 * Below is an example for the execution flow of a FOR line:
	 * 	for i=1:1:2 for j=1:1:3 s ij="dummy"
	 * When the above line is executing, the flow will be (i.e. pcurrpos will be called with):
	 * MPROF_LINESTARTorFETCH (i=1, j=1)
	 * MPROF_INTOFOR	(i=1, j=2)
	 * MPROF_INTOFOR	(j=3)
	 * MPROF_OUTOFFOR	(j end)
	 * MPROF_INTOFOR	(i=2, j=1)
	 * MPROF_INTOFOR	(j=2)
	 * MPROF_INTOFOR	(j=3)
	 * MPROF_OUTOFFOR	(j end)
	 * MPROF_OUTOFFOR	(i end)
	 * MPROF_LINESTART	(next line of code)
	 *
	 * So the loop-level counting is not straightforward.
	 * You can tell you had more than one level of FOR's only when you get MPROF_OUTOFFOR more than expected.
	 * Hence, time counting is not trivial for FOR's, and is not attempted here.
	 *
	 * inside_for_loop_state is used to count things, since you know how much time was spent only after it's spent.
	*/
	trace_entry	tmp_trc_tbl_entry;
	mprof_tree	*tmp_tblnd;
	int		tmp_int, tmp_int_a;

	assert(mprof_ptr);
	if (!mprof_ptr)
		return;	/* protect in pro build */
	TIMES(&mprof_ptr->tcurr);
	get_entryref_information(TRUE, &tmp_trc_tbl_entry);
	if (FALSE == mprof_ptr->is_tracing_ini)
	{
		mprof_ptr->is_tracing_ini = TRUE;
		mprof_ptr->curr_tblnd = mprof_ptr->head_tblnd = (mprof_tree *)new_node(&tmp_trc_tbl_entry);
	}
	if (NULL != mprof_ptr->curr_tblnd)
	{
		if ((MPROF_OUTOFFOR | MPROF_INTOFOR) & mprof_ptr->inside_for_loop_state)
		{
			/*either on the way out of FOR, or into FOR (or another iteration)*/
			LINK_NEW(mprof_ptr->curr_tblnd);
			if (MPROF_OUTOFFOR == mprof_ptr->inside_for_loop_state)
			{
				/*going OUT of a loop*/
				mprof_ptr->curr_tblnd->e.cur_loop_level--;
				if (mprof_ptr->curr_tblnd->e.cur_loop_level < 0)
				{
					/* there was another level*/
					mprof_ptr->curr_tblnd->e.cur_loop_level = 0;
					mprof_ptr->curr_tblnd->e.loop_level++;
				}
				/*update the loop counts*/
				tmp_int = mprof_ptr->curr_tblnd->e.loop_level;
				tmp_tblnd = mprof_ptr->curr_tblnd->loop_link;
				tmp_tblnd->e.loop_level = mprof_ptr->curr_tblnd->e.loop_level;
				while (0 < tmp_int)
				{
					LINK_NEW(tmp_tblnd);
					(tmp_tblnd->loop_link)->e.loop_level = tmp_tblnd->e.loop_level - 1;
					if (tmp_int == mprof_ptr->curr_tblnd->e.cur_loop_level + 1)
					{
						(tmp_tblnd->loop_link)->e.count++;
						PRINT_PROF_TREE_ELEM("INC: ");
					}
					tmp_tblnd = tmp_tblnd->loop_link;
					tmp_int--;
				}
			}

			if (MPROF_INTOFOR == mprof_ptr->inside_for_loop_state)
			{
				/*going INTO a loop*/
				/* when going into, we go all the way in*/
				if (0 == mprof_ptr->curr_tblnd->e.loop_level)
					mprof_ptr->curr_tblnd->e.loop_level = 1;
				mprof_ptr->curr_tblnd->e.cur_loop_level = mprof_ptr->curr_tblnd->e.loop_level;
			}
			UPDATE_TIME(mprof_ptr->curr_tblnd->loop_link);
			UPDATE_TIME(mprof_ptr->curr_tblnd);
			if ((MPROF_OUTOFFOR == mprof_ptr->inside_for_loop_state) && (!SAME_LINE))
				mprof_ptr->for_level = 1;
			if (MPROF_INTOFOR == mprof_ptr->inside_for_loop_state)
				mprof_ptr->for_level = 1;

			if (mprof_ptr->for_level)
			{
				/* increment the level count for all levels*/
				tmp_int = mprof_ptr->curr_tblnd->e.loop_level;
				tmp_int_a = 1; /* do the time update only for the first time*/
				tmp_tblnd = mprof_ptr->curr_tblnd;
				while (0 <= tmp_int)
				{
					LINK_NEW(tmp_tblnd);
					if (tmp_int_a)
						(tmp_tblnd->loop_link)->e.count++;
					else
						UPDATE_TIME(tmp_tblnd->loop_link);
					tmp_int_a = 0;
					tmp_tblnd = tmp_tblnd->loop_link;
					tmp_int--;
				}
			}
			if (MPROF_OUTOFFOR == mprof_ptr->inside_for_loop_state)
			{
				mprof_ptr->curr_tblnd->e.for_count = 0;
				mprof_ptr->for_level = 0;
			}

		}
		if ((MPROF_LINEFETCH + MPROF_LINESTART) & mprof_ptr->inside_for_loop_state)
		{
			UPDATE_TIME(mprof_ptr->curr_tblnd);
			MPROF_INCR_COUNT;
		}
		if ((MPROF_OUTOFFOR == inside_for_loop) && (SAME_LINE))
		{
			/*prepare next guy*/
			tmp_trc_tbl_entry.for_count = 1;
			mprof_ptr->for_level = 0;
			/*no counting here, no timing either */
		} else
			mprof_ptr->for_level = 1;
	}
	mprof_ptr->curr_tblnd = (mprof_tree *)mprof_tree_insert(mprof_ptr->head_tblnd, &tmp_trc_tbl_entry);
	PRINT_PROF_TREE("in pcurrpos");
	/*PRINT_PROF_TREE_ELEM("end pcurrpos: ");*/
	mprof_ptr->tprev = mprof_ptr->tcurr;
	mprof_ptr->inside_for_loop_state = inside_for_loop;
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
		mprof_ptr->pcavail = PROFCALLOC_DSBLKSIZE - sizeof(char *);
		memset(mprof_ptr->pcavailptr + 1, 0, mprof_ptr->pcavail);
	}
	mprof_ptr->pcavail -= n;
	assert(mprof_ptr->pcavail >= 0);
	return (char *) mprof_ptr->pcavailptr + mprof_ptr->pcavail + sizeof(char *);
}

void new_prof_frame(int dummy)
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
	struct tms		curr;

	error_def(ERR_MAXTRACELEVEL);

	if (dummy)
	{
		if (++mprof_ptr->line_prof_stack < MAX_MPROF_STACK_LEVEL)
		{
			mprof_ptr->currnd_stk[mprof_ptr->line_prof_stack] = mprof_ptr->curr_tblnd;
			mprof_ptr->time_stack[mprof_ptr->line_prof_stack] = mprof_ptr->tprev;
			mprof_ptr->loop_info_stk[mprof_ptr->line_prof_stack] = mprof_ptr->inside_for_loop_state;
		} else
		{
			if (!mprof_ptr->overflowed_levels)
			{
				gtm_putmsg(VARLSTCNT(3) ERR_MAXTRACELEVEL, 1, MAX_MPROF_STACK_LEVEL);
				mprof_ptr->overflowed_levels++;
			}
			return;
		}
		mprof_ptr->curr_tblnd = NULL;
		TIMES(&curr);
		prof_fp->sys_time = curr.tms_stime - prof_fp->sys_time;
		prof_fp->usr_time = curr.tms_utime - prof_fp->usr_time;
		psf = (stack_frame_prof *) (prof_msp -= sizeof(stack_frame_prof));
		psf->prev = prof_fp;
		psf->sys_time = curr.tms_stime;
		psf->usr_time = curr.tms_utime;
		psf->rout_name = NULL;
		psf->label_name = NULL;
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

	assert(mprof_ptr);
	if (!mprof_ptr)
		return;	/* protect in pro build */
	if (mprof_ptr->line_prof_stack >= MAX_MPROF_STACK_LEVEL)
	{
		mprof_ptr->line_prof_stack--;
		assert(mprof_ptr->overflowed_levels);
		return;
	}
	TIMES(&curr);
	if (NULL == prof_fp)
		return;
	if (!prof_fp->dummy_stack_count)
	{
		get_entryref_information(TRUE, &tmp_trc_tbl_entry);
		if (NULL == prof_fp)
			return;
		PRINT_PROF_TREE("");
		prof_fp->sys_time = curr.tms_stime - prof_fp->sys_time;
		prof_fp->usr_time = curr.tms_utime - prof_fp->usr_time;
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
		e.for_count = 0;
		t = mprof_tree_insert(mprof_ptr->head_tblnd, &tmp_trc_tbl_entry);
		/*update count and timing of quit statements (implicit or explicit)*/
		t->e.count++;
		t->e.usr_time += (curr.tms_utime - mprof_ptr->tprev.tms_utime);
		t->e.sys_time += (curr.tms_stime - mprof_ptr->tprev.tms_stime);
		t = mprof_tree_insert(mprof_ptr->head_tblnd, &e);
		/*update count and timing (from prof_fp) of frame I'm leaving*/
		t->e.count++;
		t->e.sys_time += (unsigned int)prof_fp->sys_time;
		t->e.usr_time += (unsigned int)prof_fp->usr_time;
		if (prof_fp->prev)
		{
			if (mprof_ptr->line_prof_stack > 0)
			{
				mprof_ptr->tprev = mprof_ptr->time_stack[mprof_ptr->line_prof_stack];
				mprof_ptr->curr_tblnd = mprof_ptr->currnd_stk[mprof_ptr->line_prof_stack];
				mprof_ptr->inside_for_loop_state = mprof_ptr->loop_info_stk[mprof_ptr->line_prof_stack];
				mprof_ptr->line_prof_stack--;
			} else
				GTMASSERT;
			/* move back up to parent frame */
			prof_msp = (unsigned char *)prof_fp + sizeof(stack_frame_prof);
			prof_fp = prof_fp->prev;
			prof_fp->sys_time = curr.tms_stime - prof_fp->sys_time;
			prof_fp->usr_time = curr.tms_utime - prof_fp->usr_time;
		} else
		{
			/* This should only be true only if the View command is not at
			 * the top-most stack level. In which case add profiling information
			 * for the quit statement. */
			mprof_ptr->tprev = mprof_ptr->tcurr;
			mprof_ptr->curr_tblnd = NULL;
			prof_fp = (stack_frame_prof *)prof_msp;
			save_fp = frame_pointer;
			frame_pointer = frame_pointer->old_frame_pointer;
			get_entryref_information(FALSE, NULL);
			frame_pointer = save_fp;
			if (NULL == prof_fp)
				return;
			prof_fp->prev = (stack_frame_prof *)NULL;
			prof_fp->sys_time = curr.tms_stime;
			prof_fp->usr_time = curr.tms_utime;
			prof_fp->prev = NULL;
			/*tag it so that next time, it will pick up label/routine info from current loc*/
			prof_fp->rout_name = (mident *)&above_routine;
			prof_fp->label_name = NULL;
			prof_fp->dummy_stack_count = 0;
		}
	} else
	{
		assert(prof_fp->dummy_stack_count > 0);
		prof_fp->dummy_stack_count--;
	}
	return;
}

void crt_gbl(mprof_tree *p, int info_level)
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
		spt->str.len = sizeof(MPROF_NULL_LABEL) - 1;
		spt->str.addr = (char *)pcalloc((unsigned int)spt->str.len);
		memcpy(spt->str.addr, MPROF_NULL_LABEL, spt->str.len);
	}
	mprof_ptr->gvargs.args[count++] = spt++;
	spt->mvtype = MV_STR;
	if (-1 != p->e.line_num)
	{
		tmpnum = i2asc(asc_line_num, (unsigned int) p->e.line_num);
		spt->str.len = INTCAST(tmpnum - asc_line_num);
		spt->str.addr = (char *)pcalloc((unsigned int)spt->str.len);
		memcpy(spt->str.addr, asc_line_num, spt->str.len);
		mprof_ptr->gvargs.args[count] = spt;
		count++;
		spt++;
	} else if (0 == p->e.line_num)
	{
		spt->str.len = sizeof("*unk*") - 1;
		spt->str.addr = (char *)pcalloc((unsigned int)spt->str.len);
		memcpy(spt->str.addr, "*unk*", spt->str.len);
		mprof_ptr->gvargs.args[count] = spt;
		count++;
	}
	if (info_level)
	{
		spt->mvtype = MV_STR;
		spt->str.len = sizeof(MPROF_FOR_LOOP) - 1;
		spt->str.addr = (char *)pcalloc(USIZEOF(MPROF_FOR_LOOP) - 1);
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
	tmpnum = (unsigned char *)&dataval[0];
	end = i2asc(tmpnum, p->e.count);
	tmpnum += ((end - tmpnum) > 0) ? (end - tmpnum) : (tmpnum - end);
	if (info_level < 1)
	{
		*tmpnum = ':';
		tmpnum++;
		end = i2asc(tmpnum, p->e.usr_time);
		tmpnum += ((end - tmpnum) > 0) ? (end - tmpnum) : (tmpnum - end);
#ifndef VMS
		*tmpnum = ':';
		tmpnum++;
		end = i2asc(tmpnum, p->e.sys_time);
		tmpnum += ((end - tmpnum) > 0) ? (end - tmpnum) : (tmpnum - end);
		*tmpnum = ':';
		tmpnum++;
		end = i2asc(tmpnum, p->e.sys_time + p->e.usr_time);
		tmpnum += ((end - tmpnum) > 0) ? (end - tmpnum) : (tmpnum - end);
#endif
	}
	data.mvtype = MV_STR;
	data.str.len = (((INTPTR_T)tmpnum - start_point) > 0)
		? ((int)((INTPTR_T)tmpnum - start_point)) : ((int)(start_point - (INTPTR_T)tmpnum));
	if ((mprof_ptr->overflowed_levels) && (-1 == p->e.line_num))
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

static void get_entryref_information(boolean_t line, trace_entry *tmp_trc_tbl_entry)
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
		/*The equality check in the second half of the expression below is to
		  account for the delay-slot in HP-UX for implicit quits.
		  */
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
	/* if (0 != max_label->lab_name.len && NULL == prof_fp->label_name) */
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
	tmp_trc_tbl_entry->for_count = 0;
}

static void parse_gvn(mval *gvn)
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

	error_def(ERR_NOTGBL);
	error_def(ERR_STRUNXEOR);
	error_def(ERR_VIEWNOTFOUND);
	error_def(ERR_TEXT);
	error_def(ERR_MAXNRSUBSCRIPTS);

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
	if (!ISALPHA(ch) && (ch != '%'))
		RTS_ERROR_VIEWNOTFOUND("Invalid global name");
	for ( ; (c_ref < c_top) && ('(' != *c_ref); )
	{
		ch = *mpsp++ = *c_ref++;
		if (!ISALNUM(ch))
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
				if ((c_ref < c_top) && (ISALPHA(*c_ref)))
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
				if (!ISDIGIT(ch)  &&  ch != '.'  &&  ch != '-'  &&  ch != '+')
					RTS_ERROR_VIEWNOTFOUND("Improperly formatted numeric subscript");
				if ('.' == ch)
					dot_seen = TRUE;
				*mpsp++ = *c_ref++;
				for ( ; ; )
				{
					if (c_ref == c_top)
						RTS_ERROR_VIEWNOTFOUND("Right parenthesis expected");
					if (!ISDIGIT(*c_ref))
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
	if (&var_on_cstack != var_on_cstack_ptr)
		GTMASSERT;
}
