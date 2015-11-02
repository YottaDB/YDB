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

#ifndef MPROF_H_INCLUDED
#define MPROF_H_INCLUDED
#include "xfer_enum.h"
#include "fix_xfer_entry.h"
#ifdef UNIX
#include "gtm_times.h"
#include <sys/resource.h>
#endif
#define OFFSET_LEN 			8
#define TOTAL_SIZE_OF_PROFILING_STACKS 	8388608
#define	GUARD_RING_FOR_PROFILING_STACK	1024
#ifdef GTM64
#define PROFCALLOC_DSBLKSIZE 		8192
#else
#define PROFCALLOC_DSBLKSIZE            8180
#endif

#define MAX_MPROF_TREE_HEIGHT		32

#define POPULATE_PROFILING_TABLE() { \
	/* xfer_table[xf_linefetch] = op_mproflinefetch; */	\
    /*	xfer_table[xf_linefetch] = NON_IA64_ONLY((op_mproflinefetch) IA64_ONLY(CODE_ADDRESS_ASM(op_mproflinestart));*/ \
	FIX_XFER_ENTRY(xf_linefetch, op_mproflinefetch); \
   /*	xfer_table[xf_linestart] = NON_IA64_ONLY(op_mproflinestart) IA64_ONLY(CODE_ADDRESS_ASM(op_mproflinestart)); */  \
	FIX_XFER_ENTRY(xf_linestart, op_mproflinestart) \
	FIX_XFER_ENTRY(xf_extexfun, op_mprofextexfun); \
	FIX_XFER_ENTRY(xf_extcall, op_mprofextcall); \
	FIX_XFER_ENTRY(xf_exfun, op_mprofexfun); \
	FIX_XFER_ENTRY(xf_callb, op_mprofcallb); \
	FIX_XFER_ENTRY(xf_calll, op_mprofcalll); \
	FIX_XFER_ENTRY(xf_callw, op_mprofcallw); \
	FIX_XFER_ENTRY(xf_callspl, op_mprofcallspl); \
	FIX_XFER_ENTRY(xf_callspw, op_mprofcallspw); \
	FIX_XFER_ENTRY(xf_callspb, op_mprofcallspb); \
	FIX_XFER_ENTRY(xf_forlcldob, op_mprofforlcldob); \
	FIX_XFER_ENTRY(xf_forlcldow, op_mprofforlcldow); \
	FIX_XFER_ENTRY(xf_forlcldol, op_mprofforlcldol); \
	FIX_XFER_ENTRY(xf_forchk1, op_mprofforchk1); \
}

#define CLEAR_PROFILING_TABLE() { \
	FIX_XFER_ENTRY(xf_linefetch, op_linefetch); \
	FIX_XFER_ENTRY(xf_linestart, op_linestart); \
	FIX_XFER_ENTRY(xf_extexfun, op_extexfun); \
	FIX_XFER_ENTRY(xf_extcall, op_extcall); \
	FIX_XFER_ENTRY(xf_exfun, op_exfun); \
	FIX_XFER_ENTRY(xf_callb, op_callb); \
	FIX_XFER_ENTRY(xf_callw, op_callw); \
	FIX_XFER_ENTRY(xf_calll, op_calll); \
/*	xfer_table[xf_exfun]     = op_exfun; */ \
	FIX_XFER_ENTRY(xf_callspb, op_callspb); \
	FIX_XFER_ENTRY(xf_callspw, op_callspw); \
	FIX_XFER_ENTRY(xf_callspl, op_callspl); \
	FIX_XFER_ENTRY(xf_forlcldob, op_forlcldob); \
	FIX_XFER_ENTRY(xf_forlcldow, op_forlcldow); \
	FIX_XFER_ENTRY(xf_forlcldol, op_forlcldol); \
	FIX_XFER_ENTRY(xf_forchk1, op_forchk1); \
}

#if defined(VMS) && !defined(__TIME_LOADED)
struct tms {
	int4	tms_utime;		/* user time */
	int4	tms_stime;		/* system time */
};
#endif

/* holds information identifying a line of/label in the code */
typedef struct {
	mident		*rout_name;	/* routine name */
	mident  	*label_name;	/* label name */
	signed int  	line_num;	/* line number; -1 used for generic label nodes, and -2 for overflow node */
	unsigned int	count;		/* number of executions */
	unsigned int	sys_time;	/* total system time  */
	unsigned int	usr_time;	/* total user time */
	int		loop_level;	/* nesting level; 0 for regular code and 1+ for (nested) loops */
	char		*raddr;		/* return address used in FORs to record destination after current iteration */
} trace_entry;

/* defines an mprof stack frame */
typedef struct stack_frame_prof_struct
{
	struct stack_frame_prof_struct	*prev;			/* reference to the previous frame */
	mident				*rout_name;		/* routine name */
	mident				*label_name;		/* label name */
	struct tms			start;			/* user and system time at the beginning of current measurement */
	int				dummy_stack_count;	/* number of non-label stack frames (IFs and FORs with DO) */
} stack_frame_prof;

/* defines a node of mprof tree which is an AVL tree */
typedef struct mprof_tree {
	trace_entry		e;		/* holds identifying information about this node */
	struct mprof_tree 	*link[2]; 	/* references to left and right children */
	struct mprof_tree	*loop_link; 	/* FORs attached to the node as a linked list */
	int 			desc_dir;	/* descending direction that indicates which subtree is unbalanced */
} mprof_tree;

char 	*pcalloc(unsigned int);
void	turn_tracing_on(mval *glvn);
void	turn_tracing_off(mval *);
void	new_prof_frame(int);
void 	mprof_tree_walk(mprof_tree *);
void	pcurrpos(int inside_for_loop);
void	forchkhandler(char *return_address);
void	unw_prof_frame(void);
mprof_tree *new_node(trace_entry *);
mprof_tree *new_for_node(trace_entry *, char *);
void	mprof_tree_print(mprof_tree *tree,int tabs,int longl);
mprof_tree *mprof_tree_insert(mprof_tree **, trace_entry *);
void	crt_gbl(mprof_tree *p, boolean_t is_for);
void	stack_leak_check(void);

/* functions required for the transfer table manipulations*/
int op_mproflinefetch(), op_mproflinestart();
int op_mprofextexfun(), op_mprofextcall(), op_mprofexfun();
int op_mprofcallb(), op_mprofcallw(), op_mprofcalll();
int op_mprofcallspw(), op_mprofcallspl(), op_mprofcallspb();
int op_mprofforlcldow(), op_mprofforlcldol(), op_mprofforlcldob(), op_mprofforchk1();
int op_linefetch(), op_linestart();
int op_extexfun(), op_extcall(), op_exfun(), op_forlcldo();
int op_callw(), op_calll(), op_callb();
int op_callspw(), op_callspl(), op_callspb();
int op_forlcldow(), op_forlcldol(), op_forlcldob(), op_forchk1();

#endif /* MPROF_H_INCLUDED */
