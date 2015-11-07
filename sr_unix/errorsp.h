/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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
#  define CONDSTK_INITIAL_INCR	2	/* Low initial limit for DEBUG to exercise extensions */
#else
#  define CONDSTK_INITIAL_INCR	8	/* Initial increment value used when expanding condition handler stack */
#endif
#define CONDSTK_MAX_INCR	128	/* Increment doubles each time expanded till hits this level */
#define CONDSTK_MAX_STACK	512	/* Actual max is approx 504 due to arithmetic progression */
#define CONDSTK_RESERVE		3	/* Reserve 2 frames for when process_exiting */

#define CONDITION_HANDLER(name)	ch_ret_type name(int arg)

/* Count of arguments the TPRETRY error will make available for tp_restart to use */
#define TPRESTART_ARG_CNT 6

typedef void	ch_ret_type;
typedef struct condition_handler_struct
{
	struct condition_handler_struct	*save_active_ch;	/* -> Previous active condition handler */
	boolean_t			ch_active;		/* True when *THIS* condition handler is active (not usable) */
	ch_ret_type			(*ch)();		/* Condition handler address */
	jmp_buf				jmp;			/* setjmp/longjmp buffer associated with ESTABLISH point */
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
 */
#ifdef GTM_TRIGGER
/* Note the 3rd assert makes sure we are NOT returning to a trigger-invoking frame which does not have a valid msp to
 * support a return since a call to op_gvput or op_kill does not save a return addr in the M stackframe but only in the C
 * stackframe. But if proc_act_type is non-zero we set an error frame flag and getframe instead detours to
 * error_return which deals with the module appropriately.
 */
#define MUM_TSTART		{												\
					GBLREF unsigned short	proc_act_type;							\
					GBLREF int		process_exiting;						\
																\
					assert(!process_exiting);								\
                                        CHTRACEPOINT;										\
					for ( ;(ctxt > &chnd[0]) && (ctxt->ch != &mdb_condition_handler); ctxt--);		\
					CHECKLOWBOUND(ctxt);									\
					assert((ctxt->ch == &mdb_condition_handler)						\
					       && (FALSE == ctxt->save_active_ch->ch_active));					\
					/* Absolutely critical that this *never* occur hence assertpro() */			\
					assertpro(!(SFF_IMPLTSTART_CALLD & frame_pointer->flags) || (0 != proc_act_type)	\
						  || (SFF_ETRAP_ERR & frame_pointer->flags));					\
					DBGEHND((stderr, "MUM_TSTART: Frame 0x"lvaddr" dispatched\n", frame_pointer));		\
                                        ctxt->ch_active = FALSE; 								\
					restart = mum_tstart;									\
					active_ch = ctxt;									\
					longjmp(ctxt->jmp, 1);									\
				}
#else
#define MUM_TSTART		{												\
					GBLREF int		process_exiting;						\
																\
					assert(!process_exiting);								\
                                        CHTRACEPOINT;										\
					for ( ;ctxt > &chnd[0] && ctxt->ch != &mdb_condition_handler; ctxt--); 			\
					CHECKLOWBOUND(ctxt);									\
					assert((ctxt->ch == &mdb_condition_handler)						\
					       && (FALSE == ctxt->save_active_ch->ch_active));					\
					DBGEHND((stderr, "MUM_TSTART: Frame 0x"lvaddr" dispatched\n", frame_pointer));		\
                                        ctxt->ch_active = FALSE; 								\
					restart = mum_tstart;									\
					active_ch = ctxt;									\
					longjmp(ctxt->jmp, 1);									\
				}
#endif

#define GTM_ASM_ESTABLISH	{	/* So named because gtm_asm_establish does exactly this */		\
					CHTRACEPOINT;								\
					ctxt++;									\
					if (ctxt >= (chnd_end + (!process_exiting ? 0 : CONDSTK_RESERVE)))	\
						condstk_expand();						\
                                        CHECKHIGHBOUND(ctxt);							\
                                        ctxt->save_active_ch = active_ch;					\
                                        ctxt->ch_active = FALSE;						\
					active_ch = ctxt;							\
				}
#define ESTABLISH_NOJMP(x)	{										\
					GTM_ASM_ESTABLISH;							\
					ctxt->ch = x;								\
				}

#define ESTABLISH_NOUNWIND(x)	ESTABLISH_NOJMP(x)
#define ESTABLISH_RET(x, ret)	{										\
					ESTABLISH_NOJMP(x);							\
					if (setjmp(ctxt->jmp) == -1)						\
					{									\
						REVERT;								\
						return ret;							\
					}									\
				}

#ifdef __cplusplus  /* must specify return value (if any) for C++ */
# define ESTABLISH(x, ret)	ESTABLISH_RET(x, ret)
#else
# define ESTABLISH(x)		{										\
					ESTABLISH_NOJMP(x);							\
					if (setjmp(ctxt->jmp) == -1)						\
					{									\
						REVERT;								\
						return;								\
					}									\
				}
# define ESTABLISH_NORET(x, did_long_jump)									\
				{										\
					did_long_jump = FALSE;							\
					ESTABLISH_NOJMP(x);							\
					if (setjmp(ctxt->jmp) == -1)						\
						did_long_jump = TRUE;						\
				}
#endif

#define REVERT			{										\
                                        CHTRACEPOINT;								\
					active_ch = ctxt->save_active_ch;					\
					CHECKHIGHBOUND(active_ch);						\
					CHECKLOWBOUND(active_ch);						\
					ctxt--;									\
                                        CHECKLOWBOUND(ctxt);							\
				}

#define CONTINUE		{									\
                                        CHTRACEPOINT;							\
					active_ch++;							\
                                        CHECKHIGHBOUND(active_ch);					\
                                        chnd[current_ch].ch_active = FALSE;				\
					return;								\
				}

#define DRIVECH(x)		{									\
					error_def(ERR_TPRETRY);		/* BYPASSOK */			\
					CHTRACEPOINT;							\
					if (ERR_TPRETRY != error_condition)				\
						ch_cond_core();						\
					if (NULL != active_ch)						\
					{								\
						while (active_ch >= &chnd[0])				\
						{							\
							if (!active_ch->ch_active)			\
							       break;					\
							active_ch--;					\
						}							\
						if (active_ch >= &chnd[0] && *active_ch->ch)		\
							(*active_ch->ch)(x);				\
						else							\
							ch_overrun();					\
					} else								\
					{	/* No condition handler has been ESTABLISHed yet.	\
						 * Most likely error occuring at process startup.	\
						 * Just print error and exit with error status.		\
						 */							\
						stop_image_ch();					\
					}								\
                                }

#define NEXTCH			{									\
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

/* Should never unwind a condition handler established with ESTABLISH_NOUNWIND. Currently t_ch and dbinit_ch are the only ones. */
#define UNWINDABLE(unw_ch)	((&t_ch != unw_ch->ch) && (&dbinit_ch != unw_ch->ch))
#define UNWIND(dummy1, dummy2)	{												\
					GBLREF	int		process_exiting;						\
					GBLREF	boolean_t	ok_to_UNWIND_in_exit_handling;					\
																\
					assert(!process_exiting || ok_to_UNWIND_in_exit_handling);				\
					/* When we hit an error in the midst of commit, t_ch/t_commit_cleanup should be invoked	\
					 * and clean it up before any condition handler on the stack unwinds. 			\
					 */											\
					assert(0 == have_crit(CRIT_IN_COMMIT));							\
                                        CHTRACEPOINT;										\
                                        chnd[current_ch].ch_active = FALSE;							\
					active_ch++;										\
                                        CHECKHIGHBOUND(active_ch);								\
					ctxt = active_ch;									\
					assert(UNWINDABLE(active_ch));								\
					longjmp(active_ch->jmp, -1);								\
				}

#define START_CH		int current_ch;										\
				DCL_THREADGBL_ACCESS;									\
															\
				SETUP_THREADGBL_ACCESS;									\
				CHTRACEPOINT;										\
				current_ch = (active_ch - chnd);							\
				active_ch->ch_active = TRUE;								\
				active_ch--;										\
				CHECKLOWBOUND(active_ch);								\
				DBGEHND((stderr, "%s: Condition handler entered at line %d - arg: %d  SIGNAL: %d\n",	\
				         __FILE__, __LINE__, arg, SIGNAL));

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

#define MUMPS_EXIT		{							\
					GBLREF int4 exi_condition;			\
                                        GBLREF int  mumps_status;			\
                                        CHTRACEPOINT;					\
					mumps_status = SIGNAL;				\
					exi_condition = -mumps_status;  		\
					EXIT(-exi_condition);				\
				}

#define PROCDIE(x)		_exit(x)	/* No exit handler, no cleanup, just die */
#define EXIT(x)			{					\
						exit(x);		\
				}

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
#define DUMPABLE                ( DUMP ||                                       \
				 ( SEVERITY == SEVERE && IS_GTM_ERROR(SIGNAL)   \
					&& SIGNAL != (int)ERR_OUTOFSPACE) )

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
CONDITION_HANDLER(gvtr_tpwrap_ch);
CONDITION_HANDLER(iob_io_error1);
CONDITION_HANDLER(iob_io_error2);
CONDITION_HANDLER(mu_extract_handler);
CONDITION_HANDLER(mu_extract_handler1);
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
CONDITION_HANDLER(op_fnzpeek_ch);
CONDITION_HANDLER(op_fnzpeek_getpool_ch);

#endif
