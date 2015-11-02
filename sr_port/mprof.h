/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MPROF_H_INCLUDED
#define MPROF_H_INCLUDED
#include "mdef.h"
#include "str2gvargs.h"
#include "xfer_enum.h"
#include "fix_xfer_entry.h"
#ifdef UNIX
#include "gtm_times.h"
#include <sys/resource.h>
#endif
#define OFFSET_LEN		8	/* single-pointer padding at the beginning of pcalloc allocation; 8 is chosen to both fit a
					 * pointer on 32- and 64-bit architectures and ensure proper memory alignment even when we
					 * are dealing with 32-bit compiles on 64-bit machines */
#define PROFCALLOC_DSBLKSIZE	8192	/* the size of pcalloc allocation chunks */

#define POPULATE_PROFILING_TABLE() {				\
	/* xfer_table[xf_linefetch] = op_mproflinefetch; */	\
	/* xfer_table[xf_linefetch] = NON_IA64_ONLY((op_mproflinefetch) IA64_ONLY(CODE_ADDRESS_ASM(op_mproflinestart));*/	\
	FIX_XFER_ENTRY(xf_linefetch, op_mproflinefetch);	\
	/* xfer_table[xf_linestart] = NON_IA64_ONLY(op_mproflinestart) IA64_ONLY(CODE_ADDRESS_ASM(op_mproflinestart)); */	\
	FIX_XFER_ENTRY(xf_linestart, op_mproflinestart)		\
	FIX_XFER_ENTRY(xf_extexfun, op_mprofextexfun);		\
	FIX_XFER_ENTRY(xf_extcall, op_mprofextcall);		\
	FIX_XFER_ENTRY(xf_exfun, op_mprofexfun);		\
	FIX_XFER_ENTRY(xf_callb, op_mprofcallb);		\
	FIX_XFER_ENTRY(xf_calll, op_mprofcalll);		\
	FIX_XFER_ENTRY(xf_callw, op_mprofcallw);		\
	FIX_XFER_ENTRY(xf_callspl, op_mprofcallspl);		\
	FIX_XFER_ENTRY(xf_callspw, op_mprofcallspw);		\
	FIX_XFER_ENTRY(xf_callspb, op_mprofcallspb);		\
	FIX_XFER_ENTRY(xf_forlcldob, op_mprofforlcldob);	\
	FIX_XFER_ENTRY(xf_forlcldow, op_mprofforlcldow);	\
	FIX_XFER_ENTRY(xf_forlcldol, op_mprofforlcldol);	\
	FIX_XFER_ENTRY(xf_forchk1, op_mprofforchk1);		\
}

#define CLEAR_PROFILING_TABLE() {			\
	FIX_XFER_ENTRY(xf_linefetch, op_linefetch);	\
	FIX_XFER_ENTRY(xf_linestart, op_linestart);	\
	FIX_XFER_ENTRY(xf_extexfun, op_extexfun);	\
	FIX_XFER_ENTRY(xf_extcall, op_extcall);		\
	FIX_XFER_ENTRY(xf_exfun, op_exfun);		\
	FIX_XFER_ENTRY(xf_callb, op_callb);		\
	FIX_XFER_ENTRY(xf_callw, op_callw);		\
	FIX_XFER_ENTRY(xf_calll, op_calll);		\
	/* xfer_table[xf_exfun]     = op_exfun; */	\
	FIX_XFER_ENTRY(xf_callspb, op_callspb);		\
	FIX_XFER_ENTRY(xf_callspw, op_callspw);		\
	FIX_XFER_ENTRY(xf_callspl, op_callspl);		\
	FIX_XFER_ENTRY(xf_forlcldob, op_forlcldob);	\
	FIX_XFER_ENTRY(xf_forlcldow, op_forlcldow);	\
	FIX_XFER_ENTRY(xf_forlcldol, op_forlcldol);	\
	FIX_XFER_ENTRY(xf_forchk1, op_forchk1);		\
}

typedef struct ext_tms_struct
{
	gtm_uint64_t	tms_utime;	/* user time */
	gtm_uint64_t	tms_stime;	/* system time */
	gtm_uint64_t	tms_etime;	/* elapsed time */
} ext_tms;

/* holds information identifying a line of/label in the code */
typedef struct
{
	mident		*rout_name;	/* routine name */
	mident  	*label_name;	/* label name */
	signed int  	line_num;	/* line number; -1 used for generic label nodes, and -2 for overflow node */
	unsigned 	count;		/* number of executions */
	gtm_uint64_t	sys_time;	/* total system time  */
	gtm_uint64_t	usr_time;	/* total user time */
	gtm_uint64_t	elp_time;	/* total elapsed time */
	int		loop_level;	/* nesting level; 0 for regular code and 1+ for (nested) loops */
	char		*raddr;		/* return address used in FORs to record destination after current iteration */
} trace_entry;

/* defines an mprof stack frame */
typedef struct mprof_stack_frame_struct
{
	struct mprof_stack_frame_struct	*prev;			/* reference to the previous frame */
	mident				*rout_name;		/* routine name */
	mident				*label_name;		/* label name */
	struct mprof_tree_struct	*curr_node;		/* reference to the current tree node in this frame */
	struct ext_tms_struct		start;			/* user and system time at the beginning of current measurement */
	struct ext_tms_struct		carryover;		/* user and system time to be subtracted from parent frames */
	int				dummy_stack_count;	/* number of non-label stack frames (IFs and FORs with DO) */
} mprof_stack_frame;

/* defines a node of mprof tree which is an AVL tree */
typedef struct mprof_tree_struct
{
	trace_entry			e;		/* holds identifying information about this node */
	struct mprof_tree_struct 	*link[2]; 	/* references to left and right children */
	struct mprof_tree_struct	*loop_link; 	/* FORs attached to the node as a linked list */
	int 				desc_dir;	/* descending direction that indicates which subtree is unbalanced */
	int				ins_path_hint;	/* indicates whether node was (1) or was not (-1) in the path of last
							 * insert, or was the node inserted (0) */
} mprof_tree;

typedef struct mprof_wrapper_struct
{
	struct ext_tms_struct	tprev, tcurr;
	mprof_tree		*head_tblnd, *curr_tblnd;
	int			curr_num_subscripts;
	char			**pcavailptr, **pcavailbase;
	int			pcavail;
	boolean_t		is_tracing_ini;
	mval			subsc[MAX_GVSUBSCRIPTS];
	gvargs_t		gvargs;
	mval			gbl_to_fill;
} mprof_wrapper;

STATICFNDCL void get_entryref_information(boolean_t, trace_entry *);
STATICFNDCL void parse_gvn(mval *);
#ifdef UNIX
STATICFNDCL void times_usec(ext_tms *curr);
STATICFNDCL void child_times_usec(void);
#else
STATICFNDCL void get_cputime(ext_tms *curr);
#endif
STATICFNDCL void insert_total_times(boolean_t for_process);

STATICFNDCL mprof_tree *rotate_2(mprof_tree **, int);
STATICFNDCL mprof_tree *rotate_3(mprof_tree **, int, int);
STATICFNDCL int mprof_tree_compare(mprof_tree *, trace_entry *);
STATICFNDCL void mprof_tree_rebalance_path(mprof_tree *, trace_entry *);
STATICFNDCL void mprof_tree_rebalance(mprof_tree **, trace_entry *);

void	turn_tracing_on(mval *glvn, boolean_t from_env, boolean_t save_gbl);
void	turn_tracing_off(mval *);
void	forchkhandler(char *return_address);
void	pcurrpos(void);
void	new_prof_frame(int);
void	unw_prof_frame(void);
char 	*pcalloc(unsigned int);
void 	mprof_reclaim_slots(void);
void	crt_gbl(mprof_tree *p, boolean_t is_for);
void	stack_leak_check(void);

mprof_tree	*new_node(trace_entry *);
mprof_tree	*new_for_node(trace_entry *, char *);
void		mprof_tree_walk(mprof_tree *);
mprof_tree	*mprof_tree_insert(mprof_tree **, trace_entry *);

void			mprof_stack_init(void);
mprof_stack_frame	*mprof_stack_push(void);
mprof_stack_frame	*mprof_stack_pop(void);
void			mprof_stack_free(void);

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
