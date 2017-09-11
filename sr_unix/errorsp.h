/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __ERRORSP_H__
#define __ERRORSP_H__

#include <setjmp.h>

#include "gtm_stdio.h"
#include "have_crit.h"
#include "gtmimagename.h"
#include <rtnhdr.h>
#include "stack_frame.h"

#ifdef __MVS__
#  define GTMCORENAME "gtmcore"
#endif

/* Condition handler stack definitions:
 *
 * Global: chnd      - Base of condition handler stack.
 *         chnd_end  - Top of condition handler stack.
 *	   chnd_incr - Increment for next expansion.
 *         ctxt	     - Top condition handler stack currently in use.
 *         active_ch - Top of the inactive (i.e. available for invocation) condition handler stack.
 *
 * Note that START_CH used at the top of every condition handler defined "current_ch" as a stack variable
 * (i.e. "automatic" variable in some lingos). The current_ch var in a given (C) frame always points to the
 * condition handler entry active for that frame because active_ch can change in a nested handler situation.
 * Note also that current_ch has the form of an index into the condition handler array because the stack
 * can be extended via re-allocation.
 *
 * Condition handler stack starts off at a modest CONDSTK_INITIAL_INCR frames. If needed, (to cover trigger
 * and/or spanning node stack frames which nest handlers), it can expand up to MAX_CON
 *
 * Note, if ever call-ins fully support TP processing such that each trigger frame could have it's own callin
 * in addition to the other handlers for each trigger frame, the maximums may need to be re-visited.
 */
#ifdef DEBUG
#  define CONDSTK_INITIAL_INCR	5	/* Lower initial limit for DEBUG to exercise extensions. Note that values below 5 cause
					 * issues with nested malloc()s when using certain gtmdbglvl values. */
#else
#  define CONDSTK_INITIAL_INCR	8	/* Initial increment value used when expanding condition handler stack */
#endif
#define CONDSTK_MAX_INCR	128	/* Increment doubles each time expanded till hits this level */
#define CONDSTK_MAX_STACK	512	/* Actual max is approx 504 due to arithmetic progression */
#define CONDSTK_RESERVE		4	/* Reserve 4 frames for when process_exiting */

#define CONDITION_HANDLER(name)	ch_ret_type name(int arg)

/* Count of arguments the TPRETRY error will make available for tp_restart to use */
#define TPRESTART_ARG_CNT 6

typedef void	ch_ret_type;

/* Note that the condition_handler structure layout is relied upon by assembly code (see chnd_size, chnd_* in error.si).
 * Any changes here need corresponding changes in error.si.
 */
typedef struct condition_handler_struct
{
	struct condition_handler_struct	*save_active_ch;	/* -> Previous active condition handler */
	boolean_t			ch_active;		/* True when *THIS* condition handler is active (not usable) */
	uint4				dollar_tlevel;		/* $tlevel at time of ESTABLISH; needed at UNWIND time.
								 * Used only in DEBUG code. But defined in PRO to keep error.si
								 * simple (it keeps track of offsets of members in structures
								 * and we dont want it to be conditional on PRO vs DBG).
								 */
	ch_ret_type			(*ch)();		/* Condition handler address */
	jmp_buf				jmp;			/* setjmp/longjmp buffer associated with ESTABLISH point */
	intrpt_state_t			intrpt_ok_state;	/* intrpt_ok_state at time of ESTABLISH_RET/ESTABLISH */
} condition_handler;

/* The values below usually expand as GBLREF. If CHEXPAND is defined, they will
 * expand as GBLDEF instead (as it does in gbldefs.c).
 */
#ifdef CHEXPAND
#  define GBLEXP GBLDEF
#else
#  define GBLEXP GBLREF
#endif
GBLEXP condition_handler	*chnd, *ctxt, *active_ch, *chnd_end;
GBLEXP int			severity, chnd_incr;

/* Don't do these GBLREFs if doing above GBLDEFs as this means we are in gbldefs.c where the below
 * are also defined so a REF is likely to cause problems.
 */
#ifndef CHEXPAND
GBLREF int4			error_condition;
GBLREF err_ctl			merrors_ctl;
GBLREF void			(*restart)();
GBLREF int			process_exiting;
#endif

#define WARNING		0
#define SUCCESS		1
#define ERROR		2
#define INFO		3
#define SEVERE		4
#define SEV_MSK		7

#define IS_GTM_ERROR(err) ((err & FACMASK(merrors_ctl.facnum))  &&  (MSGMASK(err, merrors_ctl.facnum) <= merrors_ctl.msg_cnt))
#define CHECKHIGHBOUND(hptr)  assert(hptr < (chnd_end + (!process_exiting ? 0 : CONDSTK_RESERVE)))
#define CHECKLOWBOUND(hptr)   assert(hptr >= (&chnd[0] - 1)) /* Low check for chnd - 1 in case last handler setup new handler */

#define SIGNAL		error_condition
#define SEVERITY	severity

#define FATALFAILURE(X)		(X > 4)
#define CHANDLER_EXISTS (active_ch >= &chnd[0])

#define MAKE_MSG_WARNING(x)	((x) & ~SEV_MSK | WARNING)
#define MAKE_MSG_SUCCESS(x)	((x) & ~SEV_MSK | SUCCESS)
#define MAKE_MSG_ERROR(x)	((x) & ~SEV_MSK | ERROR)
#define MAKE_MSG_INFO(x)	((x) & ~SEV_MSK | INFO)
#define MAKE_MSG_SEVERE(x)	((x) & ~SEV_MSK | SEVERE)

#define ERROR_RTN		error_return

/* The CHTRACEPOINT macros are in place for CH debugging if necessary */
#ifdef DEBUG
#  ifdef DEBUG_CH
#    define CHTRACEPOINT ch_trace_point();
#    ifdef CHEXPAND
void ch_trace_point() {return;}
#    endif
#  else
#    define CHTRACEPOINT
#  endif
#else
#  define CHTRACEPOINT
#endif

#define PRN_ERROR		util_cond_flush();

/* MUM_TSTART unwinds the current C-stack and restarts executing generated code from the top of the current M-stack.
 * With the introduction of call-ins, there could be multiple mdb_condition_handlers stacked up in chnd stack.
 * The active context should be reset to the youngest mdb_condition_handler created by the current gtm/call-in invocation. We have
 * two flavors depending on if triggers are enabled or not.
 *
 * Note, like the UNWIND macro below, these MUM_TSTART macros cannot disable interrupts across the longjmp() because the
 * assembler version ESTABLISH macro does not support re-enabling interrupts at this time. When that support is added, the
 * ENABLE_INTERRUPTS macro can be removed from these MUM_TSTART macros and handled instead in the longjmp() return of the
 * setjmp() call there.
 */
/* Note the 3rd assert makes sure we are NOT returning to a ANY frame which does not have a valid msp to support a return
 * since a call to op_gvput or op_kill does not save a return addr in the M stackframe but only in the C stackframe. But
 * if proc_act_type is non-zero we set an error frame flag and getframe instead detours to error_return which deals with
 * the module appropriately.
 */
#define MUM_TSTART		{												\
					GBLREF unsigned short	proc_act_type;							\
					GBLREF int		process_exiting;						\
																\
					intrpt_state_t		prev_intrpt_state;						\
																\
					assert(!multi_thread_in_use);								\
					assert(!process_exiting);								\
					CHTRACEPOINT;										\
					DEFER_INTERRUPTS(INTRPT_IN_CONDSTK, prev_intrpt_state);					\
					for ( ;(ctxt > &chnd[0]) && (ctxt->ch != &mdb_condition_handler); ctxt--);		\
					CHECKLOWBOUND(ctxt);									\
					assert((ctxt->ch == &mdb_condition_handler)						\
					       && (FALSE == ctxt->save_active_ch->ch_active));					\
					/* Absolutely critical that this *never* occur hence assertpro() */			\
					assertpro(!(SSF_NORET_VIA_MUMTSTART & frame_pointer->flags) || (0 != proc_act_type)	\
						  || (SFF_ETRAP_ERR & frame_pointer->flags));					\
					DBGEHND((stderr, "MUM_TSTART: Frame 0x"lvaddr" dispatched\n", frame_pointer));		\
					ctxt->ch_active = FALSE; 								\
					restart = mum_tstart;									\
					active_ch = ctxt;									\
					ENABLE_INTERRUPTS(INTRPT_IN_CONDSTK, prev_intrpt_state);				\
					longjmp(ctxt->jmp, 1);									\
				}

/* Assumed that when this macro is used, interrupts are disabled. One case where that is not done exists which is in
 * sr_unix/gtm_asm_establish.c (called by assembler routines). Once the assembler ESTABLISH macros support doing the
 * disable/enable of interrupts this routine can add an assert that interrupts are disabled.
 */
#define GTM_ASM_ESTABLISH	{	/* So named because gtm_asm_establish does exactly this */		\
					GBLREF uint4 dollar_tlevel;						\
														\
					assert(IS_PTHREAD_LOCKED_AND_HOLDER);					\
					CHTRACEPOINT;								\
					ctxt++;									\
					if (ctxt >= (chnd_end + (!process_exiting ? 0 : CONDSTK_RESERVE)))	\
						condstk_expand();						\
					CHECKHIGHBOUND(ctxt);							\
					ctxt->save_active_ch = active_ch;					\
					ctxt->ch_active = FALSE;						\
					DEBUG_ONLY(ctxt->dollar_tlevel = dollar_tlevel;)			\
					active_ch = ctxt;							\
				}
/* Currently the ESTABLISH_NOJMP macro is only used internal to this header file - if ever used outside this header file, it
 * needs to be protected with DEFER/ENABLE_INTERRUPTS macros.
 */
#define ESTABLISH_NOJMP(x)	{								\
					GTM_ASM_ESTABLISH;					\
					ctxt->ch = x;						\
				}

#define ESTABLISH_NOUNWIND(x)	{										\
					intrpt_state_t		prev_intrpt_state;				\
														\
					DEFER_INTERRUPTS(INTRPT_IN_CONDSTK, prev_intrpt_state);			\
					ESTABLISH_NOJMP(x);							\
					ENABLE_INTERRUPTS(INTRPT_IN_CONDSTK, prev_intrpt_state);		\
				}

#define ESTABLISH_RET(x, ret)	{										\
					intrpt_state_t		prev_intrpt_state;				\
														\
					DEFER_INTERRUPTS(INTRPT_IN_CONDSTK, prev_intrpt_state);			\
					ESTABLISH_NOJMP(x);							\
					/* Save "prev_intrpt_state" input parameter in "ctxt". This is later	\
					 * needed if/when "setjmp" returns non-zero (through longjmp). At that	\
					 * time, local variable values are not restored correctly on Solaris	\
					 * most likely because the local variable "prev_intrpt_state" defined	\
					 * in the ESTABLISH_RET macro block goes out-of-scope the moment the	\
					 * macro returns whereas on other platforms it stays in-scope most	\
					 * likely because this gets allocated in the outermost {...} of the	\
					 * calling function. Just in case this behavior changes in the future,	\
					 * we store it in a global var for all platforms (not just Solaris).	\
					 */									\
					ctxt->intrpt_ok_state = prev_intrpt_state;				\
					if (0 != setjmp(ctxt->jmp))						\
					{									\
						prev_intrpt_state = ctxt->intrpt_ok_state;			\
						REVERT;								\
						/* The only way we should reach here is if a "longjmp" happened	\
						 * inside the condition handler "x". In that case, we dont	\
						 * know the state of the global variable "intrpt_ok_state" so	\
						 * reset it to what it was before the DEFER_INTERRUPTS macro	\
						 * call in the ESTABLISH_RET macro.				\
						 */								\
						if (!multi_thread_in_use)					\
						{								\
							assert(INTRPT_OK_TO_INTERRUPT <= prev_intrpt_state);	\
							assert(INTRPT_NUM_STATES > prev_intrpt_state);		\
							intrpt_ok_state = prev_intrpt_state;			\
						}								\
						return ret;							\
					} else									\
						ENABLE_INTERRUPTS(INTRPT_IN_CONDSTK, prev_intrpt_state);	\
				}

#ifdef __cplusplus  /* must specify return value (if any) for C++ */
# define ESTABLISH(x, ret)	ESTABLISH_RET(x, ret)
#else
# define ESTABLISH(x)		{										\
					intrpt_state_t		prev_intrpt_state;				\
														\
					DEFER_INTERRUPTS(INTRPT_IN_CONDSTK, prev_intrpt_state);			\
					ESTABLISH_NOJMP(x);							\
					/* See ESTABLISH_RET macro comment for below assert */			\
					ctxt->intrpt_ok_state = prev_intrpt_state;				\
					if (0 != setjmp(ctxt->jmp))						\
					{									\
						prev_intrpt_state = ctxt->intrpt_ok_state;			\
						REVERT;								\
						/* See ESTABLISH_RET macro comment for below assert */		\
						if (!multi_thread_in_use)					\
						{								\
							assert(INTRPT_OK_TO_INTERRUPT <= prev_intrpt_state);	\
							assert(INTRPT_NUM_STATES > prev_intrpt_state);		\
							intrpt_ok_state = prev_intrpt_state;			\
						}								\
						return;								\
					} else									\
						ENABLE_INTERRUPTS(INTRPT_IN_CONDSTK, prev_intrpt_state);	\
				}
# define ESTABLISH_NORET(x, did_long_jump)									\
				{										\
					intrpt_state_t		prev_intrpt_state;				\
														\
					DEFER_INTERRUPTS(INTRPT_IN_CONDSTK, prev_intrpt_state);			\
					did_long_jump = FALSE;							\
					ESTABLISH_NOJMP(x);							\
					/* See ESTABLISH_RET macro comment for below assert */			\
					ctxt->intrpt_ok_state = prev_intrpt_state;				\
					if (0 != setjmp(ctxt->jmp))						\
					{									\
						prev_intrpt_state = ctxt->intrpt_ok_state;			\
						did_long_jump = TRUE;						\
						/* See ESTABLISH_RET macro comment for below assert */		\
						assert(INTRPT_OK_TO_INTERRUPT <= prev_intrpt_state);		\
						assert(INTRPT_NUM_STATES > prev_intrpt_state);			\
						/* Assert "intrpt_ok_state" and "prev_intrpt_state" are same in	\
						 * "dbg" but restore "intrpt_ok_state" in "pro" just in case	\
						 */								\
						assert(prev_intrpt_state == intrpt_ok_state);			\
						intrpt_ok_state = prev_intrpt_state;				\
					} else									\
						ENABLE_INTERRUPTS(INTRPT_IN_CONDSTK, prev_intrpt_state);	\
				}

# define WITH_CH(HANDLER, OP, ERR_OP)										\
MBSTART {													\
	boolean_t	ERROR_SEEN;										\
														\
	ESTABLISH_NORET(HANDLER, ERROR_SEEN);									\
	if (!ERROR_SEEN)											\
	{													\
		OP;												\
		REVERT;												\
	} else													\
	{													\
		REVERT;												\
		ERR_OP;												\
	}													\
} MBEND
#endif

#define REVERT			{										\
					intrpt_state_t		prev_intrpt_state;				\
														\
					assert(IS_PTHREAD_LOCKED_AND_HOLDER);					\
					CHTRACEPOINT;								\
					DEFER_INTERRUPTS(INTRPT_IN_CONDSTK, prev_intrpt_state);			\
					active_ch = ctxt->save_active_ch;					\
					CHECKHIGHBOUND(active_ch);						\
					CHECKLOWBOUND(active_ch);						\
					ctxt--;									\
					CHECKLOWBOUND(ctxt);							\
					ENABLE_INTERRUPTS(INTRPT_IN_CONDSTK, prev_intrpt_state);		\
				}

#define CONTINUE		{											\
					intrpt_state_t		prev_intrpt_state;					\
															\
					/* If threads are in use, a CONTINUE inside a condition-handler transitions	\
					 * the thread from error-handling to a no-error state which can cause an	\
					 * out-of-design situation because we assume that all threads which go		\
					 * through any condition-handler exit and never resume execution.		\
					 */										\
					assert(!multi_thread_in_use);							\
					CHTRACEPOINT;									\
					DEFER_INTERRUPTS(INTRPT_IN_CONDSTK, prev_intrpt_state);				\
					active_ch++;									\
					CHECKHIGHBOUND(active_ch);							\
					chnd[current_ch].ch_active = FALSE;						\
					ENABLE_INTERRUPTS(INTRPT_IN_CONDSTK, prev_intrpt_state);			\
					return;										\
				}

#define DRIVECH(x)		{										\
					intrpt_state_t		prev_intrpt_state;				\
														\
					error_def(ERR_TPRETRY);		/* BYPASSOK */				\
														\
					assert(IS_PTHREAD_LOCKED_AND_HOLDER);					\
					/* See comment before SET_FORCED_THREAD_EXIT in GTM_PTHREAD_EXIT macro	\
					 * for why the below PTHREAD_EXIT_IF_FORCED_EXIT is necessary.		\
					 */									\
					if (multi_thread_in_use)						\
						PTHREAD_EXIT_IF_FORCED_EXIT;					\
					CHTRACEPOINT;								\
					if (ERR_TPRETRY != error_condition)					\
						ch_cond_core();							\
					if (NULL != active_ch)							\
					{									\
						DEFER_INTERRUPTS(INTRPT_IN_CONDSTK, prev_intrpt_state);		\
						while (active_ch >= &chnd[0])					\
						{								\
							if (!active_ch->ch_active)				\
							       break;						\
							active_ch--;						\
						}								\
						ENABLE_INTERRUPTS(INTRPT_IN_CONDSTK, prev_intrpt_state);	\
						if (active_ch >= &chnd[0] && *active_ch->ch)			\
							(*active_ch->ch)(x);					\
						else								\
							ch_overrun();						\
					} else									\
					{	/* No condition handler has been ESTABLISHed yet.		\
						 * Most likely error occuring at process startup.		\
						 * Just print error and exit with error status.			\
						 */								\
						stop_image_ch();						\
					}									\
					assert((SUCCESS == SEVERITY) || (INFO == SEVERITY));			\
				}

#define NEXTCH			{									\
					assert(IS_PTHREAD_LOCKED_AND_HOLDER);				\
					CHTRACEPOINT;							\
					chnd[current_ch].ch_active = FALSE;				\
					DRIVECH(arg);							\
					/* If ever DRIVECH does a CONTINUE and returns back to us, we	\
					 * need to do a CONTINUE as well so we re-establish ourselves	\
					 * on the condition handler stack. This cancels out the		\
					 * START_CH (done before the NEXTCH invocation) which would	\
					 * have removed us from the condition handler stack. This way	\
					 * we restore the condition handler stack to what it was at 	\
					 * entry into the current condition handler before returning	\
					 * on a successfully handled condition. Assert that we should	\
					 * never have been in a DUMPABLE state if DRIVECH returned.	\
					 */								\
					assert(!DUMPABLE);						\
					CONTINUE;							\
				}

/* Should never unwind a condition handler established with ESTABLISH_NOUNWIND. Currently t_ch and dbinit_ch are the only ones.
 * Use function pointer (t_ch_fnptr global variable) instead of &tch (and likewise for dbinit_ch) as it will otherwise bloat
 * executables that don't need to pull in t_ch/dbinit_ch (e.g. ftok/gtcm_shmclean/dbcertify).
 */
#define UNWINDABLE(unw_ch)	((t_ch_fnptr != unw_ch->ch) && (dbinit_ch_fnptr != unw_ch->ch))
/* Note, since we are not initially changing the assembler ESTABLISH version to also include deferring/enabling of interrupts,
 * we cannot leave the interrupt block in effect during the longjmp(). But once that support is in place, we can do away with
 * re-enabling interrupts and let the longjmp() return from setjmp() take care of it.
 */
#define UNWIND(dummy1, dummy2)	{												\
					GBLREF	int			process_exiting;					\
					GBLREF	boolean_t		ok_to_UNWIND_in_exit_handling;				\
					GBLREF	volatile boolean_t	in_wcs_recover;						\
					GBLREF	uint4			dollar_tlevel;						\
					GBLREF	ch_ret_type		(*t_ch_fnptr)();      /* Function pointer to t_ch */	\
					GBLREF	ch_ret_type		(*dbinit_ch_fnptr)();/* Function pointer to dbinit_ch */\
																\
					intrpt_state_t			prev_intrpt_state;					\
																\
					/* If threads are in use, an UNWIND inside a condition-handler transitions		\
					 * the thread from error-handling to a no-error state which can cause an		\
					 * out-of-design situation because we assume that all threads which go			\
					 * through any condition-handler exit and never resume execution.			\
					 */											\
					assert(!multi_thread_in_use);								\
					assert(!process_exiting || ok_to_UNWIND_in_exit_handling);				\
					/* When we hit an error in the midst of commit, t_ch/t_commit_cleanup should be invoked	\
					 * and clean it up before any condition handler on the stack unwinds. 			\
					 */											\
					assert((0 == have_crit(CRIT_IN_COMMIT)) || in_wcs_recover);				\
					CHTRACEPOINT;										\
					DEFER_INTERRUPTS(INTRPT_IN_CONDSTK, prev_intrpt_state);					\
					chnd[current_ch].ch_active = FALSE;							\
					active_ch++;										\
					CHECKHIGHBOUND(active_ch);								\
					ctxt = active_ch;									\
					ENABLE_INTERRUPTS(INTRPT_IN_CONDSTK, prev_intrpt_state);				\
					assert(UNWINDABLE(active_ch));								\
					assert(active_ch->dollar_tlevel == dollar_tlevel);					\
					longjmp(active_ch->jmp, -1);								\
				}

/* This macro short-circuits the condition_handler for INFO or SUCCESS errors other than CTRLC and CTRLY and returns control
 * to the command after the rts_error_csa invocation, which is what the base condition handler (mdb_condition_handler or util_ch)
 * would do anyway, but if we are going to return control we don't want any condition handlers messing with state.
 * The base condition handlers and other code in the utilities have special logic to treat CTRLC and CTRLY as operator
 * actions with a semantic significance, so this macro does not intercept those.
 * In the MUMPS run-time, unless in a direct mode frame, we skip displaying the error because we don't want to mess with
 * the application's design for user interaction.
 */
#define START_CH(continue_on_success) 		/* info is a form of success */				\
	GBLREF boolean_t	ctrlc_on;								\
													\
	error_def(ERR_CTRLC);			/* BYPASSOK */						\
	error_def(ERR_CTRLY);			/* BYPASSOK */						\
													\
	int 		current_ch;									\
	intrpt_state_t	prev_intrpt_state;								\
	DCL_THREADGBL_ACCESS;										\
													\
	SETUP_THREADGBL_ACCESS;										\
	assert(IS_PTHREAD_LOCKED_AND_HOLDER);								\
	CHTRACEPOINT;											\
	DEFER_INTERRUPTS(INTRPT_IN_CONDSTK, prev_intrpt_state);						\
	current_ch = (active_ch - chnd);								\
	active_ch->ch_active = TRUE;									\
	active_ch--;											\
	CHECKLOWBOUND(active_ch);									\
	ENABLE_INTERRUPTS(INTRPT_IN_CONDSTK, prev_intrpt_state);					\
	DBGEHND((stderr, "%s: Condition handler entered at line %d - arg: %d  SIGNAL: %d\n",		\
		 __FILE__, __LINE__, arg, SIGNAL));							\
	if ((continue_on_success)									\
		&& ((SUCCESS == SEVERITY)								\
			|| ((INFO == SEVERITY) && ((int)ERR_CTRLY != SIGNAL)				\
				&& ((int)ERR_CTRLC != SIGNAL))))					\
	{												\
		if (ctrlc_on || !IS_GTM_IMAGE)								\
			PRN_ERROR;									\
		CONTINUE;										\
	}

#define MDB_START

void stop_image(void);
void stop_image_conditional_core(void);
void stop_image_no_core(void);

#define TERMINATE		{					\
					CHTRACEPOINT;			\
					if (SUPPRESS_DUMP)		\
						stop_image_no_core();	\
					else				\
						stop_image();		\
				}

#define SUPPRESS_DUMP		(created_core || dont_want_core)
#define DUMP_CORE               gtm_dump_core();

#define MUMPS_EXIT		{										\
					GBLREF int4 exi_condition;						\
					GBLREF int  mumps_status;						\
														\
					/* We are about to manipulate global variable "mumps_status"		\
					 * and "exi_condition" so assert we are not inside threaded code.	\
					 */									\
					assert(!multi_thread_in_use);						\
					CHTRACEPOINT;								\
					mumps_status = SIGNAL;							\
					exi_condition = -mumps_status;						\
					EXIT(-exi_condition);							\
				}

#define PROCDIE(x)		UNDERSCORE_EXIT(x)	/* No exit handler, no cleanup, just die */

error_def(ERR_ASSERT);
error_def(ERR_GTMASSERT);
error_def(ERR_GTMASSERT2);
error_def(ERR_GTMCHECK);
error_def(ERR_MEMORY);
error_def(ERR_STACKOFLOW);
error_def(ERR_OUTOFSPACE);

#define DUMP			(SIGNAL == (int)ERR_ASSERT			\
				 || SIGNAL == (int)ERR_GTMASSERT		\
				 || SIGNAL == (int)ERR_GTMASSERT2		\
				 || SIGNAL == (int)ERR_GTMCHECK	/* BYPASSOK */	\
				 || SIGNAL == (int)ERR_MEMORY			\
				 || SIGNAL == (int)ERR_STACKOFLOW)

/* true if above or SEVERE and GTM error (perhaps add some "system" errors) */
#define DUMPABLE                ((SEVERITY == SEVERE) && IS_GTM_ERROR(SIGNAL)						\
					&& (SIGNAL != (int)ERR_OUTOFSPACE)						\
					DEBUG_ONLY(&& (WBTEST_ENABLED(WBTEST_SKIP_CORE_FOR_MEMORY_ERROR)		\
							? (SIGNAL != (int)ERR_MEMORY) : TRUE)))

unsigned char *set_zstatus(mstr *src, int arg, unsigned char **ctxtp, boolean_t need_rtsloc);

#define SET_ZSTATUS(ctxt)	set_zstatus(&src_line_d, SIGNAL, ctxt, TRUE);

#define MSG_OUTPUT		(1)

#define EXIT_HANDLER(x)

#define SEND_CALLERID(callee) send_msg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_CALLERID, 3, LEN_AND_STR((callee)), caller_id());
#define PRINT_CALLERID util_out_print(" -- generated from 0x!XJ.", NOFLUSH, caller_id())
#define UNIX_EXIT_STATUS_MASK	0xFF

void err_init(void (*x)());
void condstk_expand(void);
void gtm_dump(void);
void gtm_dump_core(void);
void gtm_fork_n_core(void);
void ch_cond_core(void);
void ch_overrun(void);
void util_cond_flush(void);
void stop_image_ch(void);
CONDITION_HANDLER(dbopen_ch);
CONDITION_HANDLER(gtmci_ch);
CONDITION_HANDLER(gtmci_init_ch);
CONDITION_HANDLER(gtmsecshr_cond_hndlr);
CONDITION_HANDLER(gvcst_init_autoDB_ch);
CONDITION_HANDLER(gvtr_tpwrap_ch);
CONDITION_HANDLER(iob_io_error1);
CONDITION_HANDLER(iob_io_error2);
CONDITION_HANDLER(mu_cre_file_ch);
CONDITION_HANDLER(mu_extract_handler);
CONDITION_HANDLER(mu_extract_handler1);
CONDITION_HANDLER(mu_extract_handler2);
CONDITION_HANDLER(mu_rndwn_all_helper_ch);
CONDITION_HANDLER(mu_rndwn_repl_instance_ch);
CONDITION_HANDLER(mu_rndwn_replpool_ch);
CONDITION_HANDLER(mupip_upgrade_ch);
CONDITION_HANDLER(omi_dbms_ch);
CONDITION_HANDLER(rc_dbms_ch);
CONDITION_HANDLER(read_source_ch);
CONDITION_HANDLER(source_ch);
#ifdef GTM_TRIGGER
CONDITION_HANDLER(gtm_trigger_ch);
CONDITION_HANDLER(gtm_trigger_complink_ch);
CONDITION_HANDLER(op_fnztrigger_ch);
CONDITION_HANDLER(trigger_tpwrap_ch);
#endif
CONDITION_HANDLER(gvcst_redo_root_search_ch);
CONDITION_HANDLER(gvcst_data_ch);
CONDITION_HANDLER(gvcst_get_ch);
CONDITION_HANDLER(gvcst_kill_ch);
CONDITION_HANDLER(gvcst_order_ch);
CONDITION_HANDLER(gvcst_put_ch);
CONDITION_HANDLER(gvcst_query_ch);
CONDITION_HANDLER(gvcst_queryget_ch);
CONDITION_HANDLER(gvcst_zprevious_ch);

CONDITION_HANDLER(gvcst_spr_data_ch);
CONDITION_HANDLER(gvcst_spr_kill_ch);
CONDITION_HANDLER(gvcst_spr_order_ch);
CONDITION_HANDLER(gvcst_spr_zprevious_ch);
CONDITION_HANDLER(gvcst_spr_query_ch);
CONDITION_HANDLER(gvcst_spr_queryget_ch);

CONDITION_HANDLER(op_fnzpeek_ch);
CONDITION_HANDLER(op_fnzpeek_getpool_ch);

CONDITION_HANDLER(timer_cancel_ch);

CONDITION_HANDLER(trigger_upgrade_ch);

#endif
