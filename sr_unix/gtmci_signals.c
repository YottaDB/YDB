/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_unistd.h"
#include "gtmci_signals.h"
#include "gtmsiginfo.h"
#include "invocation_mode.h"

/* These routines implement signal handling protocol between gtm and
   external user (if C is base the application):
   +	by switching the active signal handlers between gtm and external user
   +	if signal received in gtm context, it is the responsibility of
   	gtm to execute external handlers.
   + 	similary, external user is responsible to save and execute the gtm
   	signal handlers if signalled in external context.
 */

/* info of the last signal handled by generic_signal_handler */
GBLREF int4 		exi_condition;
GBLREF siginfo_t        exi_siginfo;
GBLDEF gtm_sigcontext_t exi_context;

static struct sigaction *gtm_sig_h; 	/* storage for GT.M handlers */
static struct sigaction *ext_sig_h; 	/* handlers defined in external application */
static boolean_t sig_gtm_ctxt = FALSE;	/* if current signal context is of GT.M */

/* initialize all sig handlers to 'act' and save external handlers if C is the
   base routine in call-ins. Otherwise, if M is the base routine, don't retain the
   old handlers */
void 	sig_save_ext(struct sigaction* act)
{
	int	i;
	if (MUMPS_CALLIN & invocation_mode)
	{  /* C  is the base */
		gtm_sig_h = (struct sigaction*)malloc(NSIG * SIZEOF(struct sigaction));
		ext_sig_h = (struct sigaction*)malloc(NSIG * SIZEOF(struct sigaction));
		for (i = 1; i <= NSIG; i++)
			sigaction(i, act, &ext_sig_h[i-1]);
	} else {  /* GT.M is the base */
		for (i = 1; i <= NSIG; i++)
			sigaction(i, act, 0);
	}
}

void 	sig_save_gtm(void)
{
	int	i;
	if (MUMPS_CALLIN & invocation_mode) {
		for (i = 1; i <= NSIG; i++)
			sigaction(i, 0, &gtm_sig_h[i-1]);
		sig_gtm_ctxt = TRUE;
	}
}

void	sig_switch_gtm(void) /* switch to GT.M signal context */
{
	int	i;
	if ((MUMPS_CALLIN & invocation_mode) && !sig_gtm_ctxt) {
		for (i = 1; i <= NSIG; i++)
			sigaction(i, &gtm_sig_h[i-1], &ext_sig_h[i-1]);
		sig_gtm_ctxt = TRUE;
	}
}

void	sig_switch_ext(void) /* switch to external signal context */
{
	int	i;
	if ((MUMPS_CALLIN & invocation_mode) && sig_gtm_ctxt) {
		for (i = 1; i <= NSIG; i++)
			sigaction(i, &ext_sig_h[i-1], &gtm_sig_h[i-1]);
		sig_gtm_ctxt = FALSE;
	}
}

/* identify any signal received in GT.M context, and handle them in the context
   of external user */
void 	gtmci_exit_handler(void)
{
	static boolean_t 	handler_active = FALSE;
	struct sigaction 	*act, ignore;

	if ((0 >= exi_condition && NSIG < exi_condition) || !(MUMPS_CALLIN & invocation_mode) ||
			!(MUMPS_GTMCI & invocation_mode) || handler_active)
		return;
	handler_active = TRUE;
	sig_switch_ext();
	act = &ext_sig_h[exi_condition-1];
	if (SIG_IGN == act->sa_handler || SIG_DFL == act->sa_handler)
		return;
	if (act->sa_flags & SA_SIGINFO)
		(*act->sa_sigaction)(exi_condition, &exi_siginfo, &exi_context);
	else
		(*act->sa_handler)(exi_condition);
}
