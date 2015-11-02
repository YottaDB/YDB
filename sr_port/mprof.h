/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
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
#define OFFSET_LEN 			8
#define TOTAL_SIZE_OF_PROFILING_STACKS 	8388608
#define	GUARD_RING_FOR_PROFILING_STACK	1024
#ifdef __ia64
#define PROFCALLOC_DSBLKSIZE 		8192
#else
#define PROFCALLOC_DSBLKSIZE            8180
#endif

#define MAX_MPROF_TREE_HEIGHT		32

/*entry points recognized by pcurrpos*/
#define MPROF_OUTOFFOR	0x1
#define MPROF_INTOFOR	0x2
#define MPROF_LINEFETCH	0x4
#define MPROF_LINESTART	0x8

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
	FIX_XFER_ENTRY(xf_forloop, op_mprofforloop); \
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
	FIX_XFER_ENTRY(xf_forloop, op_forloop); \
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
