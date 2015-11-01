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

#ifndef _MPROF_H
#define _MPROF_H

#define OFFSET_LEN 			8
#define TOTAL_SIZE_OF_PROFILING_STACKS 	8388608
#define	GUARD_RING_FOR_PROFILING_STACK	1024
#define PROFCALLOC_DSBLKSIZE 		8180
#define MAX_MPROF_TREE_HEIGHT		32

#ifdef UNIX
#define TIMES 				times
#elif defined(VMS)
#define TIMES 				get_cputime
#endif

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

struct trace_entry {
	unsigned char  	rout_name[9];
	unsigned char  	label_name[9];
	unsigned char  	line_num[9];
	unsigned int	count;
	unsigned int 	for_count;
	unsigned int	sys_time;
	unsigned int	usr_time;
	int		loop_level;
	int		cur_loop_level;
};

struct mprof_tree{
	struct 	trace_entry	e;
	struct  mprof_tree 	*link[2];
	struct mprof_tree	*loop_link;
	int 		bal;	/* Balance factor */
	unsigned int	cache; 	/* Used during insertion */
};

typedef struct  stack_frame_prof_struct
{
	struct stack_frame_prof_struct *prev;
	char            rout_name[9];
	char            label_name[9];
	char		filler[2];
	int		dummy_stack_count;
	unsigned long   usr_time;
	unsigned long   sys_time;
} stack_frame_prof;

#ifdef VMS
struct tms {
	int4	tms_utime;		/* user time */
	int4	tms_stime;		/* system time */
};
#endif

char 	*pcalloc(unsigned int);
void	turn_tracing_on(mval *glvn);
void	turn_tracing_off(mval *);
void	get_entryref_information(boolean_t, struct trace_entry *);
void	new_prof_frame(int);
void	parse_gvn(mval *);
void 	mprof_tree_walk(struct mprof_tree *);
void 	*mprof_tree_find_node(struct mprof_tree *, struct trace_entry);
void	pcurrpos(int inside_for_loop);
void	pcfree(void);
void	unw_prof_frame(void);
struct 	mprof_tree *mprof_tree_insert(struct mprof_tree *, struct trace_entry);
struct 	mprof_tree *new_node(struct trace_entry);
void	mprof_tree_print(struct mprof_tree *tree,int tabs,int longl);
#ifdef VMS
void    get_cputime(struct tms *);
#endif
void	crt_gbl(struct mprof_tree *p, int info_level);

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

#endif
