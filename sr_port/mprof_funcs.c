/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
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
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <unistd.h>
#elif defined(VMS)
#include <ssdef.h>
#include <descrip.h>
#include <jpidef.h>
#endif

#include "gtm_string.h"
#include "error.h"
#include "rtnhdr.h"
#include "stack_frame.h"
#include "stringpool.h"
#include "subscript.h"
#include "svnames.h"
#include "mprof.h"
#include "outofband.h"
#include "xfer_enum.h"
#include "op.h"
#include "callg.h"

GBLREF	int			(*xfer_table[])();
GBLREF 	boolean_t		is_tracing_on;
GBLREF 	stack_frame		*frame_pointer;
GBLREF 	unsigned char 		*profstack_base, *profstack_top, *prof_msp, *profstack_warn;
GBLREF 	spdesc 			stringpool;
GBLREF 	unsigned char 		*prof_stackptr;
GBLREF 	stack_frame_prof 	*prof_fp;
GBLREF 	mval			dollar_job;
GBLREF	uint4			process_id;

#define	MAX_MPROF_STACK_LEVEL	256
#ifdef 	MPROF_DEBUGGING
#define PRINT_PROF_TREE		mprof_tree_print(head_tblnd,0,1)
#define PRINT_PROF_TREE_ELEM	mprof_tree_print(curr_tblnd,0,-1)
#else
#define PRINT_PROF_TREE
#define PRINT_PROF_TREE_ELEM
#endif

static struct tms		tprev, tcurr;
static struct tms		time_stack[MAX_MPROF_STACK_LEVEL];
static struct mprof_tree	*head_tblnd, *curr_tblnd;
static struct mprof_tree	*currnd_stk[MAX_MPROF_STACK_LEVEL];
static unsigned int		loop_info_stk[MAX_MPROF_STACK_LEVEL]; /*for DO's*/
static unsigned int		for_level;
static int			inside_for_loop_state;
static int			line_prof_stack = 0;
static int			curr_num_subscripts;
static char			**pcavailptr, **pcavailbase;
static int			pcavail;
static boolean_t		is_tracing_ini;
static mval			subsc[MAX_GVSUBSCRIPTS];
static struct
{
	int4	count;
	mval	*args[MAX_GVSUBSCRIPTS + 1];
} op_gvargs;
static mval 			gbl_to_fill;

#define MPROF_NULL_LABEL "^"
#define MPROF_FOR_LOOP	"FOR_LOOP"
#define UPDATE_TIME(x)	x->e.usr_time += (tcurr.tms_utime - tprev.tms_utime);\
			x->e.sys_time += (tcurr.tms_stime - tprev.tms_stime);
#define MPROF_INCR_COUNT		curr_tblnd->e.count += 1
#define LINK_NEW(x) 	if (NULL == x->loop_link) \
				x->loop_link = (struct mprof_tree *)new_node(x->e)
#define SAME_LINE	(0 == (	strcmp((char *)tmp_trc_tbl_entry.rout_name, (char *)curr_tblnd->e.rout_name) ||  \
				strcmp((char *)tmp_trc_tbl_entry.label_name, (char *)curr_tblnd->e.label_name) ||  \
				strcmp((char *)tmp_trc_tbl_entry.line_num, (char *)curr_tblnd->e.line_num)))


int	turn_tracing_on(mval *glvn)
{
	struct tms		curr;
	struct trace_entry	tmp_trc_tbl_entry;
	/* For some reason the construct below and/or the strcpy calls made GTMSHR writeable on Vax
	   which means it was no longer shareable. Doing this differently makes it all still work.
	   Tiz a kludge authored by Satya..
	   struct mprof_tree p = {{"*dummy**", "*dummy**", "*dummy*", 0, 0, 0}, 0 , 0 }; */

	error_def(ERR_NOTGBL);
	if (is_tracing_on)
		return 0;
	if (!glvn->str.len || '^' != *(signed char *)&glvn->str.addr[0])
		rts_error(VARLSTCNT(4) ERR_NOTGBL, 2, glvn->str.len, glvn->str.addr);
	/*gbl_to_fill = glvn;*/
	memcpy(&gbl_to_fill, glvn, sizeof(gbl_to_fill));
	if (!pcavailbase)
	{	pcavailbase = (char **) malloc(PROFCALLOC_DSBLKSIZE);
		*pcavailbase = 0;
	}
	pcavailptr = pcavailbase;
	pcavail = PROFCALLOC_DSBLKSIZE - sizeof(char *);
	memset(pcavailptr + 1, 0, pcavail);
	TIMES(&curr);
	prof_stackptr = (unsigned char *)malloc(TOTAL_SIZE_OF_PROFILING_STACKS);
	prof_msp = profstack_base = prof_stackptr + (TOTAL_SIZE_OF_PROFILING_STACKS - GUARD_RING_FOR_PROFILING_STACK);
	profstack_top = prof_stackptr;
	profstack_warn = profstack_top + GUARD_RING_FOR_PROFILING_STACK;
	prof_fp = (stack_frame_prof *) (prof_msp -= sizeof(stack_frame_prof));
	get_entryref_information(FALSE, NULL);
	curr_tblnd = head_tblnd = (struct mprof_tree *)new_node(tmp_trc_tbl_entry);
	prof_fp->prev = (stack_frame_prof *)NULL;
	prof_fp->sys_time = curr.tms_stime;
	prof_fp->usr_time = curr.tms_utime;
	prof_fp->dummy_stack_count = 0;
	POPULATE_PROFILING_TABLE();
	is_tracing_on = TRUE;
	inside_for_loop_state = 0;
	for_level = 0;
}

int turn_tracing_off (mval *glvn)
{
	if (FALSE == is_tracing_on)
		return 0;
	PRINT_PROF_TREE;
	TIMES(&tcurr);
	if (NULL == glvn)
		parse_glvn(&gbl_to_fill);
	else
		parse_glvn(glvn);
	is_tracing_on = is_tracing_ini = FALSE;
	mprof_tree_walk(head_tblnd);
	free(prof_stackptr);
	pcfree();
	CLEAR_PROFILING_TABLE();
}

void    pcurrpos(int inside_for_loop)
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
	 * MPROF_LINESTARTorFETCH (i=1,j=1)
	 * MPROF_INTOFOR	(i=1,j=2)
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
	struct trace_entry	tmp_trc_tbl_entry;
	struct mprof_tree	*tmp_tblnd;
	int			tmp_int, tmp_int_a;

	TIMES(&tcurr);
	get_entryref_information(TRUE, &tmp_trc_tbl_entry);
	if (FALSE == is_tracing_ini)
	{
		is_tracing_ini = TRUE;
		curr_tblnd = head_tblnd = (struct mprof_tree *)new_node(tmp_trc_tbl_entry);
	}
	if (NULL != curr_tblnd)
	{
		if ((MPROF_OUTOFFOR + MPROF_INTOFOR) & inside_for_loop_state)
		{
			/*either on the way out of FOR, or into FOR (or another iteration)*/
			LINK_NEW(curr_tblnd);
			if (MPROF_OUTOFFOR == inside_for_loop_state)
			{
				/*going OUT of a loop*/
				curr_tblnd->e.cur_loop_level--;
				if (curr_tblnd->e.cur_loop_level < 0)
				{
					/* there was another level*/
					curr_tblnd->e.cur_loop_level = 0;
					curr_tblnd->e.loop_level++;
				}
				/*update the loop counts*/
				tmp_int = curr_tblnd->e.loop_level;
				tmp_tblnd=curr_tblnd->loop_link;
				tmp_tblnd->e.loop_level=curr_tblnd->e.loop_level;
				while (0 < tmp_int)
				{
					LINK_NEW(tmp_tblnd);
					(tmp_tblnd->loop_link)->e.loop_level = tmp_tblnd->e.loop_level - 1;
					if (tmp_int == curr_tblnd->e.cur_loop_level+1)
						(tmp_tblnd->loop_link)->e.count++;
					tmp_tblnd=tmp_tblnd->loop_link;
					tmp_int--;
				}
			}

			if (MPROF_INTOFOR == inside_for_loop_state)
			{
				/*going INTO a loop*/
				/* when going into, we go all the way in*/
				if (0 == curr_tblnd->e.loop_level) curr_tblnd->e.loop_level = 1;
				curr_tblnd->e.cur_loop_level = curr_tblnd->e.loop_level;
			}
			UPDATE_TIME(curr_tblnd->loop_link);
			UPDATE_TIME(curr_tblnd);
			if ((MPROF_OUTOFFOR == inside_for_loop_state) && (!SAME_LINE)) for_level = 1;
			if (MPROF_INTOFOR == inside_for_loop_state) for_level = 1;

			if (for_level)
			{
				/* increment the level count for all levels*/
				tmp_int = curr_tblnd->e.loop_level;
				tmp_int_a = 1; /* do the time update only for the first time*/
				tmp_tblnd=curr_tblnd;
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
			if (MPROF_OUTOFFOR == inside_for_loop_state)
				{curr_tblnd->e.for_count = 0; for_level=0;}

		}
		if ((MPROF_LINEFETCH + MPROF_LINESTART) & inside_for_loop_state)
		{
			UPDATE_TIME(curr_tblnd);
			MPROF_INCR_COUNT;

		}
			if ((MPROF_OUTOFFOR == inside_for_loop) && (SAME_LINE))
			{
				/*prepare next guy*/
				tmp_trc_tbl_entry.for_count = 1;
				for_level = 0;
				/*no counting here, no timing either */
			}
			else
				for_level = 1;
	}
	PRINT_PROF_TREE_ELEM;
	curr_tblnd = (struct mprof_tree *)mprof_tree_insert(head_tblnd, tmp_trc_tbl_entry);
	tprev = tcurr;
	inside_for_loop_state = inside_for_loop;
}

char *pcalloc(unsigned int n)
{
	char **x;

	n = ((n + 3) & ~3); /* make sure that it is quad-word aligned */
	if (n > pcavail)
	{
		if (*pcavailptr)
			pcavailptr = (char ** ) *pcavailptr;
		else
		{
			x = (char **) malloc(PROFCALLOC_DSBLKSIZE);
			*pcavailptr = (char *) x;
			pcavailptr = x;
			*pcavailptr = 0;
		}
		pcavail = PROFCALLOC_DSBLKSIZE - sizeof(char *);
		memset(pcavailptr + 1, 0, pcavail);
	}
	pcavail -= n;
	assert(pcavail >= 0);
	return (char *) pcavailptr + pcavail + sizeof(char *);
}

void pcfree(void)
{
	pcavailptr = pcavailbase;
	pcavail = PROFCALLOC_DSBLKSIZE - sizeof(char *);
	return;
}

void	new_prof_frame(int dummy)
{
	stack_frame_prof	*psf;
	struct tms		curr;

	if (dummy)
	{
		if (++line_prof_stack < MAX_MPROF_STACK_LEVEL)
		{
			currnd_stk[line_prof_stack] = curr_tblnd;
			time_stack[line_prof_stack] = tprev;
			loop_info_stk[line_prof_stack] = inside_for_loop_state;
		}
		curr_tblnd = NULL;
		TIMES(&curr);
		prof_fp->sys_time = curr.tms_stime - prof_fp->sys_time;
		prof_fp->usr_time = curr.tms_utime - prof_fp->usr_time;
		psf = (stack_frame_prof *) (prof_msp -= sizeof(stack_frame_prof));
		psf->prev = prof_fp;
		psf->sys_time = curr.tms_stime;
		psf->usr_time = curr.tms_utime;
		psf->rout_name[0] = '\0';
		psf->label_name[0] = '\0';
		prof_fp = psf;
		prof_fp->dummy_stack_count = 0;
	}
	else
		prof_fp->dummy_stack_count += 1;
	return;
}

void unw_prof_frame (void)
{
	struct trace_entry	e;
	struct trace_entry	tmp_trc_tbl_entry;
	struct mprof_tree	*t;
	struct tms		curr;
	stack_frame		*save_fp;

	TIMES(&curr);
	if (NULL == prof_fp)
		return;
	if (!prof_fp->dummy_stack_count)
	{
		get_entryref_information(TRUE, &tmp_trc_tbl_entry);
		prof_fp->sys_time = curr.tms_stime - prof_fp->sys_time;
		prof_fp->usr_time = curr.tms_utime - prof_fp->usr_time;
		strcpy((char *)e.label_name, prof_fp->label_name);
		strcpy((char *)e.rout_name, prof_fp->rout_name);
		strcpy((char *)e.line_num, "*dlin*");
		e.for_count = 0;
		t = mprof_tree_insert(head_tblnd, tmp_trc_tbl_entry);
		t->e.count += 1;
		t = mprof_tree_insert(head_tblnd, e);
		t->e.count += 1;
		/* A hack because the sys_time and the user time are
		 * improperly maintained in the system. It has been discovered
		 * that the cpu-time used by the process decreases, and in all cases
		 * the absolute value is taken.
		 */
		prof_fp->sys_time = (prof_fp->sys_time < 0) ? -prof_fp->sys_time : prof_fp->sys_time;
		prof_fp->usr_time = (prof_fp->usr_time < 0) ? -prof_fp->usr_time : prof_fp->usr_time;
		assert(prof_fp->sys_time >= 0 && prof_fp->usr_time >= 0);
		t->e.sys_time += prof_fp->sys_time;
		t->e.usr_time += prof_fp->usr_time;
		if (prof_fp->prev)
		{
			if (line_prof_stack > 0)
			{
				tprev = time_stack[line_prof_stack];
				curr_tblnd = currnd_stk[line_prof_stack];
				inside_for_loop_state = loop_info_stk[line_prof_stack];
				line_prof_stack--;
			} else
				GTMASSERT;
			prof_msp = (unsigned char *)prof_fp + sizeof(stack_frame_prof);
			prof_fp = prof_fp->prev;
			prof_fp->sys_time += curr.tms_stime;
			prof_fp->usr_time += curr.tms_utime;
		}
		else
		{
			/* This should only be true only if the View command is not at
			 * the top-most stack level. In which case add profiling information
			 * for the quit statement. */
			if (NULL != curr_tblnd)
			{
				curr_tblnd->e.usr_time += (tcurr.tms_utime - tprev.tms_utime);
				curr_tblnd->e.sys_time += (tcurr.tms_stime - tprev.tms_stime);
				curr_tblnd->e.count += 1;
			}
			tprev = tcurr;
			curr_tblnd = NULL;
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
			prof_fp->dummy_stack_count = 0;
		}
	}
	else
	{
		assert(prof_fp->dummy_stack_count > 0);
		prof_fp->dummy_stack_count--;
	}
	return;
}

void	crt_gbl(struct mprof_tree *p, int info_level)
{
	/* Write the data into the global
	 */
	char		*c_top, *c_ref, ch;
	int		count, arg_index, subsc_len, start_point;
	mval		data;
	char		dataval[56];
	unsigned char	subsval[4];
	unsigned char	*tmpnum, *end;
	mval		*spt;

	error_def(ERR_GVINVALID);

	if (0 == p->e.count)
		return;
	count = op_gvargs.count;
	spt = &subsc[count];
	/* Global name --> ^PREFIX(<OPTIONAL ARGUMENTS>, "rout-name", "label-name", "line-num", "forloop") */
	spt->mvtype = MV_STR;
	spt->str.len = strlen((char *)p->e.rout_name);
	spt->str.addr = (char *)pcalloc(spt->str.len+1);
	memcpy(spt->str.addr, p->e.rout_name, strlen((char *)p->e.rout_name));
	op_gvargs.args[count++] = spt++;
	spt->mvtype = MV_STR;
	if (p->e.label_name[0] != '\0')
	{
		spt->str.len = strlen((char *)p->e.label_name);
		spt->str.addr = (char *)pcalloc(spt->str.len+1);
		memcpy(spt->str.addr, p->e.label_name, spt->str.len);
	}
	else
	{	/* place holder before first label */
		spt->str.len = sizeof(MPROF_NULL_LABEL) - 1;
		spt->str.addr = (char *)pcalloc(sizeof(MPROF_NULL_LABEL));
		memcpy(spt->str.addr, MPROF_NULL_LABEL, spt->str.len);
	}
	op_gvargs.args[count++] = spt++;
	spt->mvtype = MV_STR;
	spt->str.len = strlen((char *)p->e.line_num);
	if (strcmp((char *)p->e.line_num, "*dlin*"))
	{
		spt->str.addr = (char *)pcalloc(spt->str.len+1);
		memcpy(spt->str.addr, p->e.line_num, strlen((char *)p->e.line_num));
		op_gvargs.args[count] = spt;
		count++;
		spt++;
	}
	else if ('\0' == p->e.line_num)
	{
		spt->str.len = strlen("*unk*");
		spt->str.addr = (char *)pcalloc(spt->str.len+1);
		memcpy(spt->str.addr, "*unk*", spt->str.len);
		op_gvargs.args[count] = spt;
		count++;
	}
	if (info_level)
	{
		spt->mvtype = MV_STR;
		spt->str.len = strlen(MPROF_FOR_LOOP);
		spt->str.addr = (char *)pcalloc(sizeof(MPROF_FOR_LOOP));
		memcpy(spt->str.addr, MPROF_FOR_LOOP, spt->str.len);
		op_gvargs.args[count++] = spt++;
		/*write for level into the subscript as well*/
		spt->mvtype = MV_STR;
		tmpnum = i2asc(subsval, p->e.loop_level);
		*tmpnum = '\0';
		spt->str.len = strlen((char *)subsval);
		spt->str.addr = (char *)pcalloc(spt->str.len+1);
		memcpy(spt->str.addr, subsval, spt->str.len);
		op_gvargs.args[count++] = spt++;
	}
	op_gvargs.count = count;
	callg((int(*)())op_gvname, &op_gvargs);
	op_gvargs.count = curr_num_subscripts;
	/* Data --> "count:cpu-time in user mode:cpu-time in sys mode" */
	start_point = (int)&dataval[0];
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
	data.str.len = (((int )tmpnum - start_point) > 0) ? ((int )tmpnum - start_point) : (start_point - (int )tmpnum);
	data.str.addr = (char *)pcalloc(data.str.len);
	memcpy(data.str.addr, dataval, data.str.len);
	op_gvput(&data);
	return;
}

void	get_entryref_information(boolean_t line, struct trace_entry *tmp_trc_tbl_entry)
{
	boolean_t	line_reset;
	lbl_tables	*max_label, *label_table, *last_label;
	rhdtyp		*routine;
	stack_frame	*fp;
	int		status;
	unsigned char	*addr, *out_addr;
	unsigned char	temp[OFFSET_LEN];
	uint4		*line_table, *last_line, len, ct;
	uint4		offset, in_addr_offset;
	unsigned long	user_time, system_time;

	line_reset = FALSE;
	for (fp = frame_pointer; fp; fp = fp->old_frame_pointer)
	{
		if ((unsigned char *) fp->rvector + fp->rvector->ptext_ptr <= fp->mpc &&
			fp->mpc < (unsigned char *) fp->rvector + fp->rvector->vartab_ptr)
		{
			if (line_reset || ((unsigned char*) fp->rvector + fp->rvector->current_rhead_ptr == fp->mpc))
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
	routine = (rhdtyp *)((unsigned char *) routine + routine->current_rhead_ptr);
	label_table = (lbl_tables *)((char *) routine + routine->labtab_ptr);
	last_label = label_table + routine->labtab_len;
	max_label = label_table++;
	while (label_table < last_label)
	{
		if (addr > (unsigned char *) routine + *((int4 *) ((char *) routine + label_table->lab_ln_ptr)))
		{
			if (max_label->lab_ln_ptr <= label_table->lab_ln_ptr)
				max_label = label_table;
		}
		label_table++;
	}
	line_table = (uint4 *)((char *) routine + max_label->lab_ln_ptr);
	len = mid_len(&max_label->lab_name);
	if (len)
	{
		if (line)
			memcpy((*tmp_trc_tbl_entry).label_name, &max_label->lab_name.c[0], len);
		if ('\0' == prof_fp->label_name[0])
		{
			memcpy(&prof_fp->label_name[0], &max_label->lab_name.c[0], len);
			(*prof_fp).label_name[len] = '\0';
		}
	}
	if (line)
		(*tmp_trc_tbl_entry).label_name[len] = '\0';
	len = mid_len(&routine->routine_name);
	if (line)
	{
		memcpy((*tmp_trc_tbl_entry).rout_name, &routine->routine_name.c[0], len);
		(*tmp_trc_tbl_entry).rout_name[len] = '\0';
	}
	memcpy(prof_fp->rout_name, routine->routine_name.c, len);
	prof_fp->rout_name[len] = '\0';
	if (!line)
		return;
	offset = 0;
	in_addr_offset = addr - (unsigned char *) routine;
	last_line = (uint4 *)((char *) routine + routine->lnrtab_ptr);
	last_line += routine->lnrtab_len;
	for( ; ++line_table < last_line ; offset++)
	{
		if (in_addr_offset <= *line_table)
			break;
	}
	if (offset)
	{
		ct = OFFSET_LEN;
		for ( ; ct > 0; )
		{
			temp[--ct] = (offset % 10) + '0';
			if (0 == (offset /= 10))
				break;
		}
		len = OFFSET_LEN - ct;
		memcpy ((*tmp_trc_tbl_entry).line_num, &temp[ct], len);
		(*tmp_trc_tbl_entry).line_num[len] = '\0';
	} else
	{
		(*tmp_trc_tbl_entry).line_num[0] = '0';
		(*tmp_trc_tbl_entry).line_num[1] = '\0';
	}
	(*tmp_trc_tbl_entry).for_count = 0;
}

void parse_glvn(mval *glvn)
{
	boolean_t 		dot_seen;
	mval			*spt;
	char			*mpsp;			/* mprof stringpool pointer */
	signed char		*c_top, *c_ref, ch;
	unsigned short		dollarc_val, i;
	unsigned int		count = 0;
	static mstr		mprof_mstr;

	error_def(ERR_NOTGBL);
	error_def(ERR_LPARENREQD);
	error_def(ERR_GVINVALID);
	error_def(ERR_NUMUNXEOR);
	error_def(ERR_STRUNXEOR);
	error_def(ERR_DLRCUNXEOR);
	error_def(ERR_DLRCTOOBIG);
	error_def(ERR_EORNOTFND);
	error_def(ERR_RPARENREQD);
	error_def(ERR_TEXT);

	if (FALSE == is_tracing_on)
		return;
	c_ref = (signed char *)&glvn->str.addr[0];
	c_top = (signed char *)c_ref + glvn->str.len;
	if (!glvn->str.len || '^' != *c_ref++)
		rts_error(VARLSTCNT(4) ERR_NOTGBL, 2, glvn->str.len, glvn->str.addr);
	if (mprof_mstr.len < 4 * glvn->str.len)
	{	/* We are going to return an array of mvals pointing to global-name and subscripts.
		 * We should never be needing more than 4 * glvn->str.len since the only expandable entity
		 * 	that can be passed is $j which uses up atleast 3 characters (including '_' or ',')
		 *	and expands to a maximum of 10 characters (see djbuff in getjobname.c).
		 */
		if (mprof_mstr.len)
		{
			assert(mprof_mstr.addr);
			free(mprof_mstr.addr);
		}
		mprof_mstr.len = 4 * glvn->str.len;
		mprof_mstr.addr = (char *)malloc(mprof_mstr.len);
	}
	mpsp = mprof_mstr.addr;
	TIMES(&tcurr);
	/* Parse the global variable passed to insert the information */
	spt = &subsc[0];
	spt->mvtype = MV_STR;
	spt->str.addr = mpsp;
	ch = *mpsp++ = *c_ref++;
	if ((ch < 'A' || ch > 'Z') && (ch != '%') && (ch < 'a' || ch > 'z'))
		rts_error(VARLSTCNT(4) ERR_GVINVALID, 2, glvn->str.len, glvn->str.addr);
	for ( ; (c_ref < c_top) && ('(' != *c_ref); )
	{
		ch = *mpsp++ = *c_ref++;
		if ((ch < 'A' || ch > 'Z')  &&  (ch < 'a' || ch > 'z')  &&  (ch < '0' || ch > '9'))
			rts_error(VARLSTCNT(4) ERR_GVINVALID, 2, glvn->str.len, glvn->str.addr);
	}
	spt->str.len = (long)mpsp - (long)spt->str.addr;
	op_gvargs.args[count++] = spt++;
	spt->str.addr = (char *)mpsp;
	/* Process subscripts if any */
	if (c_ref++ < c_top)
	{
		for ( ; ; )
		{
			spt->mvtype = MV_STR;
			ch = *c_ref;
			if ('\"' == ch)
			{
				c_ref++;
				for ( ; ; )
				{
					if (c_ref == c_top)
						rts_error(VARLSTCNT(4) ERR_STRUNXEOR, 2, glvn->str.len, glvn->str.addr);
					if (('\"' == *c_ref) && (*++c_ref != '\"'))
						break;
					*mpsp++ = *c_ref++;
				}
			} else if ('$' == ch)
			{
				if (++c_ref == c_top)
					rts_error(VARLSTCNT(4) ERR_DLRCUNXEOR, 2, glvn->str.len, glvn->str.addr);
				if (*c_ref != 'J' && *c_ref != 'j')
					rts_error(VARLSTCNT(8) ERR_DLRCUNXEOR, 2, glvn->str.len, glvn->str.addr,
								ERR_TEXT, 2, RTS_ERROR_TEXT("Intrinsic value passed not $j"));
				c_ref++; 	/* Past 'J' */
				assert(10 > dollar_job.str.len);	/* to take care of 4 * glvn->str.len allocation above */
				memcpy(mpsp, dollar_job.str.addr, dollar_job.str.len);
				mpsp += dollar_job.str.len;
			} else
			{
				dot_seen = FALSE;
				if ((ch > '9' || ch < '0')  &&  ch != '.'  &&  ch != '-'  &&  ch != '+')
					rts_error(VARLSTCNT(4) ERR_NUMUNXEOR, 2, glvn->str.len, glvn->str.addr);
				if ('.' == ch)
					dot_seen = TRUE;
				*mpsp++ = *c_ref++;
				for ( ; ; )
				{
					if (c_ref == c_top)
						rts_error(VARLSTCNT(4) ERR_NUMUNXEOR, 2, glvn->str.len, glvn->str.addr);
					if (*c_ref > '9'  ||  *c_ref < '0')
					{
						if (*c_ref != '.')
							break;
						else if (!dot_seen)
							dot_seen = TRUE;
						else
							rts_error(VARLSTCNT(4) ERR_NUMUNXEOR, 2, glvn->str.len, glvn->str.addr);
					}
					*mpsp++ = *c_ref++;
				}
			}
			if ('_' == *c_ref)
			{
				c_ref++;
				continue;
			}
			spt->str.len = (long)mpsp - (long)spt->str.addr;
			op_gvargs.args[count++] = spt++;
			if (*c_ref != ',')
				break;
			spt->str.addr = mpsp;
			c_ref++;
		}
		if (*c_ref++ != ')')
			rts_error(VARLSTCNT(4) ERR_RPARENREQD, 2, glvn->str.len, glvn->str.addr);
		if (c_ref < c_top)
			rts_error(VARLSTCNT(4) ERR_EORNOTFND, 2, glvn->str.len, glvn->str.addr);
	}
	assert((char *)mpsp <= mprof_mstr.addr + mprof_mstr.len);	/* Ensure we haven't overrun the malloced buffer */
	curr_num_subscripts = op_gvargs.count = count;
}

#if defined(VMS)
void	get_cputime (struct tms *curr)
{
	int4	cpu_time_used;
	int	status;
	int	jpi_code = JPI$_CPUTIM;

	 if ((status = lib$getjpi(&jpi_code, &process_id, 0, &cpu_time_used, 0, 0)) != SS$_NORMAL)
		rts_error(VARLSTCNT(1)  status );
	curr->tms_utime = cpu_time_used;
	curr->tms_stime = 0;
}
#endif
