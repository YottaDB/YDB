/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
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

#define CONDITION_HANDLER(name)	ch_ret_type name(int arg)
#define MAX_HANDLERS 20
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
GBLEXP condition_handler	chnd[MAX_HANDLERS], *ctxt, *active_ch;
GBLEXP int4			severity;
GBLEXP int4			error_condition;

GBLREF char 			util_outbuff[];

LITREF err_ctl			merrors_ctl;

#define WARNING		0
#define SUCCESS		1
#define ERROR		2
#define INFO		3
#define SEVERE		4
#define SEV_MSK		7

#define IS_GTM_ERROR(err) ((err & FACMASK(merrors_ctl.facnum))  &&  (MSGMASK(err, merrors_ctl.facnum) <= merrors_ctl.msg_cnt))
#define CHECKHIGHBOUND(hptr)  assert(&chnd[MAX_HANDLERS] > hptr);
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

#define GETLASTBYTE(X)		(X & 0xf)

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
 * The active context should be reset to the youngest mdb_condition_handler created by the current gtm/call-in invocation
 */
#define MUM_TSTART		{					\
                                        CHTRACEPOINT;			\
					for ( ;ctxt > &chnd[0] && ctxt->ch != &mdb_condition_handler; ctxt--)	; \
					assert(ctxt->ch == &mdb_condition_handler && FALSE == ctxt->save_active_ch->ch_active); \
                                        ctxt->ch_active = FALSE; 	\
					restart = mum_tstart;		\
					active_ch = ctxt;		\
					longjmp(ctxt->jmp, 1);		\
				}

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

#define DRIVECH(x)		{						\
                                        error_def(ERR_TPRETRY);			\
                                        CHTRACEPOINT;				\
                                        if (ERR_TPRETRY != error_condition)	\
                                                ch_cond_core();			\
                                        while (active_ch >= &chnd[0])		\
                                        {					\
                                                if (!active_ch->ch_active)	\
                                                       break; 			\
                                                active_ch--;			\
					}					\
                                        if (active_ch >= &chnd[0] && *active_ch->ch) \
					        (*active_ch->ch)(x);		\
                                        else					\
						ch_overrun();			\
                                }

#define NEXTCH			{					\
                                        CHTRACEPOINT;			\
                                        current_ch->ch_active = FALSE;	\
                                        DRIVECH(arg);			\
					return;				\
				}

#define UNWIND(dummy1, dummy2)	{					\
                                        CHTRACEPOINT;			\
                                        current_ch->ch_active = FALSE;	\
					active_ch++;			\
                                        CHECKHIGHBOUND(active_ch);	\
					ctxt = active_ch;		\
					longjmp(active_ch->jmp, -1);	\
				}

#define START_CH		condition_handler *current_ch;		\
		                {					\
                                        CHTRACEPOINT;			\
                                        current_ch = active_ch;		\
                                        current_ch->ch_active = TRUE;	\
					active_ch--;			\
                                        CHECKLOWBOUND(active_ch);	\
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

#define MUMPS_EXIT		{					\
					GBLREF int4 exi_condition;      \
                                        CHTRACEPOINT;			\
					mumps_status = SIGNAL;		\
					exi_condition = -mumps_status;  \
					EXIT(-exi_condition);		\
				}

#define PROCDIE(x)		_exit(x)
#define EXIT(x)			{					\
						exit(x);		\
				}

#define DUMP			(   SIGNAL == (int)ERR_ASSERT		\
				 || SIGNAL == (int)ERR_GTMASSERT	\
				 || SIGNAL == (int)ERR_GTMCHECK		\
				 || SIGNAL == (int)ERR_STACKOFLOW )

/* true if above or SEVERE and GTM error (perhaps add some "system" errors) */
#define DUMPABLE                ( DUMP ||                                       \
				 ( SEVERITY == SEVERE && IS_GTM_ERROR(SIGNAL)   \
					&& SIGNAL != (int)ERR_OUTOFSPACE) )

unsigned char *set_zstatus(mstr *src, int arg, unsigned char **ctxtp);

#define SET_ZSTATUS(ctxt)	set_zstatus(&src_line_d, SIGNAL, ctxt);

#define MSG_OUTPUT		(1)

GBLREF void			(*restart)();

#define EXIT_HANDLER(x)

#define SEND_CALLERID(callee) send_msg(VARLSTCNT(5) ERR_CALLERID, 3, LEN_AND_STR((callee)), caller_id());
#define PRINT_CALLERID util_out_print(" -- generated from 0x!XL.", NOFLUSH, caller_id())
#define UNIX_EXIT_STATUS_MASK	0xFF

void err_init(void (*x)());
void gtm_dump(void);
void gtm_dump_core(void);
void gtm_fork_n_core(void);
void ch_cond_core(void);
void ch_overrun(void);
void util_cond_flush(void);
CONDITION_HANDLER(dbopen_ch);
CONDITION_HANDLER(gtmsecshr_cond_hndlr);
CONDITION_HANDLER(mu_extract_handler);
CONDITION_HANDLER(mu_extract_handler1);
CONDITION_HANDLER(mupip_set_file_ch);
CONDITION_HANDLER(iob_io_error1);
CONDITION_HANDLER(iob_io_error2);
CONDITION_HANDLER(omi_dbms_ch);
CONDITION_HANDLER(rc_dbms_ch);
CONDITION_HANDLER(read_source_ch);
CONDITION_HANDLER(source_ch);
CONDITION_HANDLER(gtmci_init_ch);
CONDITION_HANDLER(gtmci_ch);

#endif
