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

#ifndef __ERRORSP_H__
#define __ERRORSP_H__

#include <setjmp.h>

#include "gtm_stdio.h"

#ifdef __MVS__
#  define GTMCORENAME "gtmcore"
#endif

/* Define maximum condition handlers. For utilities that do not use triggers (dse, lke, and such) we
   only need a basic number of condition handlers. But if triggers are supported, not only the GT.M
   runtime but MUPIP and the GTCM servers too need to be able to nest which means several triggers
   per allowed level plus extra handlers for that last level.
*/
#define MAX_HANDLERS 15		/* should be enough for dse/lke etc. */
#ifdef GTM_TRIGGER
#  define MAX_MUMPS_HANDLERS (MAX_HANDLERS + 20 + 15 + (GTM_TRIGGER_DEPTH_MAX * 2))	/* Allow for callins plus many trigr lvls */
#else
#  define MAX_MUMPS_HANDLERS (MAX_HANDLERS + 20)					/* to allow upto 10 nested callin lvls */
#endif

#define CONDITION_HANDLER(name)	ch_ret_type name(int arg)

/* Count of arguments the TPRETRY error will make available for tp_restart to use */
#define TPRESTART_ARG_CNT 6

typedef void	ch_ret_type;
typedef struct condition_handler_struct
{
	struct condition_handler_struct	*save_active_ch;
	boolean_t			ch_active;
	ch_ret_type			(*ch)();
	jmp_buf				jmp;
} condition_handler;

/* The values below usually expand as GBLREF. If CHEXPAND is defined, they will
   expand as GBLDEF instead (as it does in err_init.c) */
#ifdef CHEXPAND
#  define GBLEXP GBLDEF
#else
#  define GBLEXP GBLREF
#endif
GBLEXP condition_handler	*chnd, *ctxt, *active_ch, *chnd_end;
GBLEXP int4			severity;

/* Don't do these GBLREFs if doing above GBLDEFs as this means we are in gbldefs.c where the below
   are also defined so a REF is likely to cause problems */
#ifndef CHEXPAND
GBLREF int4			error_condition;
GBLREF char 			util_outbuff[];
GBLREF err_ctl			merrors_ctl;
GBLREF void			(*restart)();
#endif

#define WARNING		0
#define SUCCESS		1
#define ERROR		2
#define INFO		3
#define SEVERE		4
#define SEV_MSK		7

#define IS_GTM_ERROR(err) ((err & FACMASK(merrors_ctl.facnum))  &&  (MSGMASK(err, merrors_ctl.facnum) <= merrors_ctl.msg_cnt))
#define CHECKHIGHBOUND(hptr)  assert(chnd_end > hptr);
#define CHECKLOWBOUND(hptr)   assert(hptr >= (&chnd[0] - 1));

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
/* Note the 2nd assert makes sure we are NOT returning to a trigger-invoking frame which does not have a valid msp to
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
					for ( ;ctxt > &chnd[0] && ctxt->ch != &mdb_condition_handler; ctxt--)	; 		\
					assert(ctxt->ch == &mdb_condition_handler && FALSE == ctxt->save_active_ch->ch_active);	\
					/* Absolutely critical that this *never* occur hence GTMASSERT */			\
					if ((SFF_TRIGR_CALLD & frame_pointer->flags) && (0 == proc_act_type)			\
					    && !(SFF_ETRAP_ERR & frame_pointer->flags)) GTMASSERT;				\
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
					for ( ;ctxt > &chnd[0] && ctxt->ch != &mdb_condition_handler; ctxt--)	; 		\
					assert(ctxt->ch == &mdb_condition_handler && FALSE == ctxt->save_active_ch->ch_active);	\
					DBGEHND((stderr, "MUM_TSTART: Frame 0x"lvaddr" dispatched\n", frame_pointer));		\
                                        ctxt->ch_active = FALSE; 								\
					restart = mum_tstart;									\
					active_ch = ctxt;									\
					longjmp(ctxt->jmp, 1);									\
				}
#endif


#define ESTABLISH_RET(x,ret)	{					\
                                        CHTRACEPOINT;			\
					ctxt++;				\
                                        CHECKHIGHBOUND(ctxt);		\
                                        ctxt->save_active_ch = active_ch; \
                                        ctxt->ch_active = FALSE;	\
					active_ch = ctxt;		\
					ctxt->ch = x;			\
					if (setjmp(ctxt->jmp) == -1)	\
					{				\
						REVERT;			\
						return ret;		\
					}				\
				}

#ifdef __cplusplus  /* must specify return value (if any) for C++ */
#define ESTABLISH(x,ret)	{					\
                                        CHTRACEPOINT;			\
					ctxt++;				\
                                        CHECKHIGHBOUND(ctxt);		\
                                        ctxt->save_active_ch = active_ch; \
                                        ctxt->ch_active = FALSE;	\
					active_ch = ctxt;		\
					ctxt->ch = x;			\
					if (setjmp(ctxt->jmp) == -1)	\
					{				\
						REVERT;			\
						return ret;		\
					}				\
				}
#else
#define ESTABLISH(x)		{					\
                                        CHTRACEPOINT;			\
					ctxt++;				\
                                        CHECKHIGHBOUND(ctxt);		\
                                        ctxt->save_active_ch = active_ch; \
                                        ctxt->ch_active = FALSE;	\
					active_ch = ctxt;		\
					ctxt->ch = x;			\
					if (setjmp(ctxt->jmp) == -1)	\
					{				\
						REVERT;			\
						return;			\
					}				\
				}
#endif

#define REVERT			{					\
                                        CHTRACEPOINT;			\
					active_ch = ctxt->save_active_ch; \
					ctxt--;				\
                                        CHECKLOWBOUND(ctxt);		\
				}

#define CONTINUE		{					\
                                        CHTRACEPOINT;			\
					active_ch++;			\
                                        CHECKHIGHBOUND(active_ch);	\
                                        current_ch->ch_active = FALSE;	\
					return;				\
				}

#define DRIVECH(x)		{									\
					error_def(ERR_TPRETRY);						\
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

#define NEXTCH			{					\
                                        CHTRACEPOINT;			\
                                        current_ch->ch_active = FALSE;	\
                                        DRIVECH(arg);			\
					return;				\
				}

#define UNWIND(dummy1, dummy2)	{									\
					GBLREF	int		process_exiting;			\
					GBLREF	boolean_t	ok_to_UNWIND_in_exit_handling;		\
													\
					assert(!process_exiting || ok_to_UNWIND_in_exit_handling);	\
                                        CHTRACEPOINT;							\
                                        current_ch->ch_active = FALSE;					\
					active_ch++;							\
                                        CHECKHIGHBOUND(active_ch);					\
					ctxt = active_ch;						\
					longjmp(active_ch->jmp, -1);					\
				}

#define START_CH		condition_handler *current_ch;								\
				DCL_THREADGBL_ACCESS;									\
															\
				SETUP_THREADGBL_ACCESS;									\
				CHTRACEPOINT;										\
				current_ch = active_ch;									\
				current_ch->ch_active = TRUE;								\
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

#define PROCDIE(x)		_exit(x)
#define EXIT(x)			{					\
						exit(x);		\
				}

#define DUMP			(SIGNAL == (int)ERR_ASSERT			\
				 || SIGNAL == (int)ERR_GTMASSERT		\
				 || SIGNAL == (int)ERR_GTMCHECK	/* BYPASSOK */	\
				 || SIGNAL == (int)ERR_MEMORY			\
				 || SIGNAL == (int)ERR_STACKOFLOW )

/* true if above or SEVERE and GTM error (perhaps add some "system" errors) */
#define DUMPABLE                ( DUMP ||                                       \
				 ( SEVERITY == SEVERE && IS_GTM_ERROR(SIGNAL)   \
					&& SIGNAL != (int)ERR_OUTOFSPACE) )

unsigned char *set_zstatus(mstr *src, int arg, unsigned char **ctxtp, boolean_t need_rtsloc);

#define SET_ZSTATUS(ctxt)	set_zstatus(&src_line_d, SIGNAL, ctxt, TRUE);

#define MSG_OUTPUT		(1)

#define EXIT_HANDLER(x)

#define SEND_CALLERID(callee) send_msg(VARLSTCNT(5) ERR_CALLERID, 3, LEN_AND_STR((callee)), caller_id());
#define PRINT_CALLERID util_out_print(" -- generated from 0x!XJ.", NOFLUSH, caller_id())
#define UNIX_EXIT_STATUS_MASK	0xFF

void err_init(void (*x)());
void gtm_dump(void);
void gtm_dump_core(void);
void gtm_fork_n_core(void);
void ch_cond_core(void);
void ch_overrun(void);
void util_cond_flush(void);
void stop_image_ch(void);
CONDITION_HANDLER(dbopen_ch);
CONDITION_HANDLER(gtmsecshr_cond_hndlr);
CONDITION_HANDLER(mu_extract_handler);
CONDITION_HANDLER(mu_extract_handler1);
CONDITION_HANDLER(mupip_upgrade_ch);
CONDITION_HANDLER(iob_io_error1);
CONDITION_HANDLER(iob_io_error2);
CONDITION_HANDLER(omi_dbms_ch);
CONDITION_HANDLER(rc_dbms_ch);
CONDITION_HANDLER(read_source_ch);
CONDITION_HANDLER(source_ch);
CONDITION_HANDLER(gtmci_init_ch);
CONDITION_HANDLER(gtmci_ch);
CONDITION_HANDLER(gvtr_tpwrap_ch);
#ifdef GTM_TRIGGER
CONDITION_HANDLER(trigger_tpwrap_ch);
CONDITION_HANDLER(gtm_trigger_ch);
CONDITION_HANDLER(gtm_trigger_complink_ch);
CONDITION_HANDLER(op_fnztrigger_ch);
#endif

#endif
