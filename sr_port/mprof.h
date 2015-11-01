/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MPROF_H_INCLUDED
#define MPROF_H_INCLUDED

#define OFFSET_LEN 			8
#define TOTAL_SIZE_OF_PROFILING_STACKS 	8388608
#define	GUARD_RING_FOR_PROFILING_STACK	1024
#define PROFCALLOC_DSBLKSIZE 		8180
#define MAX_MPROF_TREE_HEIGHT		32

/*entry points recognized by pcurrpos*/
#define MPROF_OUTOFFOR	0x1
#define MPROF_INTOFOR	0x2
#define MPROF_LINEFETCH	0x4
#define MPROF_LINESTART	0x8

#define POPULATE_PROFILING_TABLE() { \
	xfer_table[xf_linefetch] = op_mproflinefetch; \
	xfer_table[xf_linestart] = op_mproflinestart; \
	xfer_table[xf_extexfun]	 = op_mprofextexfun; \
	xfer_table[xf_extcall]   = op_mprofextcall; \
	xfer_table[xf_exfun]     = op_mprofexfun; \
	xfer_table[xf_callb]     = op_mprofcallb; \
	xfer_table[xf_calll]     = op_mprofcalll; \
	xfer_table[xf_callw]     = op_mprofcallw; \
	xfer_table[xf_callspl]   = op_mprofcallspl; \
	xfer_table[xf_callspw]   = op_mprofcallspw; \
	xfer_table[xf_callspb]   = op_mprofcallspb; \
	xfer_table[xf_forlcldob] = op_mprofforlcldob; \
	xfer_table[xf_forlcldow] = op_mprofforlcldow; \
	xfer_table[xf_forlcldol] = op_mprofforlcldol; \
	xfer_table[xf_forloop]   = op_mprofforloop; \
}

#define CLEAR_PROFILING_TABLE() { \
	xfer_table[xf_linefetch] = op_linefetch; \
	xfer_table[xf_linestart] = op_linestart; \
	xfer_table[xf_extexfun]  = op_extexfun; \
	xfer_table[xf_extcall]   = op_extcall; \
	xfer_table[xf_exfun]     = op_exfun; \
	xfer_table[xf_callb]     = op_callb; \
	xfer_table[xf_callw]     = op_callw; \
	xfer_table[xf_calll]     = op_calll; \
	xfer_table[xf_exfun]     = op_exfun; \
	xfer_table[xf_callspb]   = op_callspb; \
	xfer_table[xf_callspw]   = op_callspw; \
	xfer_table[xf_callspl]   = op_callspl; \
	xfer_table[xf_forlcldob] = op_forlcldob; \
	xfer_table[xf_forlcldow] = op_forlcldow; \
	xfer_table[xf_forlcldol] = op_forlcldol; \
	xfer_table[xf_forloop]   = op_forloop; \
}

typedef struct {
	mident		*rout_name;
	mident  	*label_name;
	signed int  	line_num;	/* it's actually an unsigned value, but -1 is used as the impossible line */
	unsigned int	count;
	unsigned int 	for_count;
	unsigned int	sys_time;
	unsigned int	usr_time;
	int		loop_level;
	int		cur_loop_level;
} trace_entry;

typedef struct mprof_tree {
	trace_entry		e;
	struct  mprof_tree 	*link[2];
	struct mprof_tree	*loop_link;
	int 		bal;	/* Balance factor */
	unsigned int	cache; 	/* Used during insertion */
} mprof_tree;

typedef struct stack_frame_prof_struct
{
	struct stack_frame_prof_struct *prev;
	mident		*rout_name;
	mident		*label_name;
	int		dummy_stack_count;
	unsigned long   usr_time;
	unsigned long   sys_time;
} stack_frame_prof;

#if defined(VMS) && !defined(__TIME_LOADED)
struct tms {
	int4	tms_utime;		/* user time */
	int4	tms_stime;		/* system time */
};
#endif

char 	*pcalloc(unsigned int);
void	turn_tracing_on(mval *glvn);
void	turn_tracing_off(mval *);
void	new_prof_frame(int);
void 	mprof_tree_walk(mprof_tree *);
void	pcurrpos(int inside_for_loop);
void	unw_prof_frame(void);
mprof_tree *mprof_tree_insert(mprof_tree *, trace_entry *);
mprof_tree *new_node(trace_entry *);
void	mprof_tree_print(mprof_tree *tree,int tabs,int longl);
void	crt_gbl(mprof_tree *p, int info_level);
void	stack_leak_check(void);

/* functions required for the transfer table manipulations*/
int op_mproflinefetch(), op_mproflinestart();
int op_mprofextexfun(), op_mprofextcall(), op_mprofexfun();
int op_mprofcallb(), op_mprofcallw(), op_mprofcalll();
int op_mprofcallspw(), op_mprofcallspl(), op_mprofcallspb();
int op_mprofforlcldow(), op_mprofforlcldol(), op_mprofforlcldob(), op_mprofforloop();
int op_linefetch(), op_linestart();
int op_extexfun(), op_extcall(), op_exfun(), op_forlcldo();
int op_callw(), op_calll(), op_callb();
int op_callspw(), op_callspl(), op_callspb();
int op_forlcldow(), op_forlcldol(), op_forlcldob(), op_forloop();

#endif /* MPROF_H_INCLUDED */
